#include "tls_interface.hpp"
#include <string.h>

tls_interface::tls_interface(on_connect_t&& on_connect, data_consumer_t&& on_recv_data, on_error_t&& on_error, on_close_t&& on_close) : 
	net_interface(std::move(on_connect), std::bind(&tls_interface::consume_ct_data, this, std::placeholders::_1, std::placeholders::_2), 
		std::move(on_error), std::move(on_close)), consume_pt_data(std::move(on_recv_data))
{ 
}

tls_interface::~tls_interface()
{
}

struct ssl_err_tok
{
	char* ptr;
	size_t len;
};

bool parse_canonical_ssl_error(char* input, ssl_err_tok out[9])
{
	char* ptr = input;
	out[0].ptr = input;
	for(size_t i=1; i < 9; i++)
	{
		ptr=strchr(ptr,':');
		if(ptr == nullptr)
			return false;
		out[i-1].len = ptr - out[i-1].ptr; 
		ptr[0] = '\0';
		ptr++;
		out[i].ptr = ptr;
	}
	//null or \n
	out[8].len = strcspn(ptr, "\n");
	return true;
}

void tls_interface::get_tls_error()
{
	BIO* err_bio = BIO_new(BIO_s_mem());
	ERR_print_errors(err_bio);
	
	const char* out = nullptr;
	char* buf;
	size_t len = BIO_get_mem_data(err_bio, &buf);
	ssl_err_tok tok[9];
	if(buf != nullptr && parse_canonical_ssl_error(buf, tok))
		out = tok[5].ptr;
	else
		out = "Unknown TLS error.";
	
	notify_error(out);
	BIO_free(err_bio);
}

SSL_CTX* make_ssl_ctx()
{
	SSL_CTX* ssl_ctx;
	if((ssl_ctx = SSL_CTX_new(TLS_client_method())) == nullptr) 
		return nullptr;
	
	SSL_CTX_set_options(ssl_ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1);
	return ssl_ctx;
}

void tls_interface::on_connect()
{
	static SSL_CTX* ssl_ctx = make_ssl_ctx();

	handshake_complete = false;
	assert(ssl == nullptr && app_bio == nullptr && ssl_bio == nullptr);

	if(ssl_ctx == nullptr || (ssl = SSL_new(ssl_ctx)) == nullptr)
	{
		get_tls_error();
		do_shutdown();
		return;
	}

	if(BIO_new_bio_pair(&ssl_bio, 0, &app_bio, 0) != 1) 
	{
		get_tls_error();
		do_shutdown();
		return;
	}

	SSL_set_bio(ssl, ssl_bio, ssl_bio);
	SSL_set_connect_state(ssl);

	uv_read_start(reinterpret_cast<uv_stream_t*>(&h_sock), alloc_recv_buffer, on_socket_read_st);
	do_ssl_handshake();
}

void tls_interface::do_ssl_handshake()
{
	int rv;
	if((rv = SSL_do_handshake(ssl)) != 1)
	{
		switch(SSL_get_error(ssl, rv))
		{
			case SSL_ERROR_WANT_READ:
			case SSL_ERROR_WANT_WRITE:
				break;
			default:
				get_tls_error();
				do_shutdown();
				break;
		}
	}
	flush_tls_pending();
	if(rv == 1)
	{
		handshake_complete = true;
		const EVP_MD* digest;
		unsigned char md[EVP_MAX_MD_SIZE];
		unsigned int dlen;
		
		digest = EVP_get_digestbyname("sha256");
		if(digest == nullptr)
		{
			get_tls_error();
			do_shutdown();
			return;
		}

		X509* cert = SSL_get_peer_certificate(ssl);
		if(cert == nullptr)
		{
			get_tls_error();
			do_shutdown();
			return;
		}

		if(X509_digest(cert, digest, md, &dlen) != 1)
		{
			X509_free(cert);
			get_tls_error();
			do_shutdown();
			return;
		}

		BIO *bmem, *b64;
		b64 = BIO_new(BIO_f_base64());
		bmem = BIO_new(BIO_s_mem());
		
		BIO_puts(bmem, "SHA256:");
		b64 = BIO_push(b64, bmem);
		BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
		BIO_write(b64, md, dlen);
		BIO_flush(b64);

		char* b64_md = nullptr;
		size_t b64_len = BIO_get_mem_data(bmem, &b64_md);

		notify_connect(b64_md);

		BIO_free_all(b64);
		X509_free(cert);
	}
}

void tls_interface::flush_tls_pending()
{
	int nwrt = BIO_pending(app_bio);
	if(nwrt == 0)
		return;

	uint32_t buflen;
	uint32_t idx;
	char* buf = get_send_buffer(buflen, idx);

	if(nwrt > buflen)
	{
		notify_error("overflow in flush_tls_pending");
		do_shutdown();
		return;
	}

	if(BIO_read(app_bio, buf, nwrt) != nwrt)
	{
		get_tls_error();
		do_shutdown();
		return;
	}

	net_interface::finalize_socket_write(idx, nwrt);
}

ssize_t tls_interface::consume_ct_data(const char* data, size_t len)
{
	ssize_t rv = BIO_write(app_bio, data, len);
	if(rv <= 0)
	{
		get_tls_error();
		return -1;
	}

	if(!handshake_complete)
	{
		do_ssl_handshake();
		return rv;
	}

	int r = SSL_read(ssl, pt_buf+pt_pos, sizeof(pt_buf)-pt_pos);
	if ( r == 0 )
		return -1; //shutdown

	if( r > 0)
	{
		pt_pos += r;
		if(!consume_loop(consume_pt_data, pt_buf, pt_pos))
			return -1;
	}
	else
	{
		int err = SSL_get_error(ssl, r);
		if(err != SSL_ERROR_WANT_WRITE && err != SSL_ERROR_WANT_READ)
		{
			get_tls_error();
			return -1;
		}
	}

	flush_tls_pending();
	return rv;
}

void tls_interface::do_socket_write(const char* data, int len)
{
	int i;
	for(int offset = 0; offset < len; offset += i )
	{
		//handle error condition
		i = SSL_write(ssl, data + offset, len - offset);
		if(i <= 0)
		{
			get_tls_error();
			do_shutdown();
		}
		flush_tls_pending();
	}
}

void tls_interface::do_shutdown()
{
	if(shutting_down)
		return;
	SSL_shutdown(ssl);
	flush_tls_pending();
	net_interface::do_shutdown();
}

void tls_interface::on_socket_closed()
{
	if(ssl != nullptr)
	{
		SSL_free(ssl); /* implicitly frees ssl_bio */
		ssl = nullptr;
		ssl_bio = nullptr;
	}

	if(app_bio != nullptr)
	{
		BIO_free(app_bio);
		app_bio = nullptr;
	}

	pt_pos = 0;
	net_interface::on_socket_closed();
}
