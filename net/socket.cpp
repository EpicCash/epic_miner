#include "socket.hpp"
#include "console/console.hpp"
#include "executor.hpp"

#ifndef CONF_NO_TLS
#include <openssl/err.h>
#include <openssl/opensslconf.h>
#include <openssl/ssl.h>

#ifndef OPENSSL_THREADS
#error OpenSSL was compiled without thread support
#endif
#endif

inline void base_socket::print_error_s(const char* err_msg)
{
	executor::inst().log_error(err_msg);
	printer::inst().print(out_colours::K_RED, err_msg);
}

plain_socket::plain_socket()
{
	hSocket = INVALID_SOCKET;
	pSockAddr = nullptr;
}

bool plain_socket::set_hostname(const char* sAddr)
{
	char sAddrMb[256];
	char *sTmp, *sPort;

	sock_closed = false;
	size_t ln = strlen(sAddr);
	if(ln >= sizeof(sAddrMb))
	{
		print_error_s("CONNECT error: Pool address overflow.");
		return false;
	}

	memcpy(sAddrMb, sAddr, ln);
	sAddrMb[ln] = '\0';

	if((sTmp = strstr(sAddrMb, "//")) != nullptr)
	{
		sTmp += 2;
		memmove(sAddrMb, sTmp, strlen(sTmp) + 1);
	}

	if((sPort = strchr(sAddrMb, ':')) == nullptr)
	{
		print_error_s("CONNECT error: Pool port number not specified, please use format <hostname>:<port>.");
		return false;
	}

	sPort[0] = '\0';
	sPort++;

	addrinfo hints = {0};
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	pAddrRoot = nullptr;
	int err;
	if((err = getaddrinfo(sAddrMb, sPort, &hints, &pAddrRoot)) != 0)
	{
		print_error(err);
		return false;
	}

	addrinfo* ptr = pAddrRoot;
	std::vector<addrinfo*> ipv4;
	std::vector<addrinfo*> ipv6;

	while(ptr != nullptr)
	{
		if(ptr->ai_family == AF_INET)
			ipv4.push_back(ptr);
		if(ptr->ai_family == AF_INET6)
			ipv6.push_back(ptr);
		ptr = ptr->ai_next;
	}

	if(ipv4.empty() && ipv6.empty())
	{
		freeaddrinfo(pAddrRoot);
		pAddrRoot = nullptr;
		print_error_s("CONNECT error: I found some DNS records but no IPv4 or IPv6 addresses.");
		return false;
	}
	else if(!ipv4.empty() && ipv6.empty())
		pSockAddr = ipv4[rand() % ipv4.size()];
	else if(ipv4.empty() && !ipv6.empty())
		pSockAddr = ipv6[rand() % ipv6.size()];
	else if(!ipv4.empty() && !ipv6.empty())
		pSockAddr = ipv4[rand() % ipv4.size()];

	hSocket = socket(pSockAddr->ai_family, pSockAddr->ai_socktype, pSockAddr->ai_protocol);

	if(hSocket == INVALID_SOCKET)
	{
		freeaddrinfo(pAddrRoot);
		pAddrRoot = nullptr;
		print_error();
		return false;
	}

	int flag = 1;
	/* If it fails, it fails, we won't loose too much sleep over it */
	setsockopt(hSocket, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(int));

	return true;
}

bool plain_socket::connect()
{
	sock_closed = false;
	int ret = ::connect(hSocket, pSockAddr->ai_addr, (int)pSockAddr->ai_addrlen);

	if(ret != 0)
		print_error();

	freeaddrinfo(pAddrRoot);
	pAddrRoot = nullptr;

	return ret == 0;
}

int plain_socket::recv(char* buf, unsigned int len)
{
	if(sock_closed)
		return 0;

	int ret = ::recv(hSocket, buf, len, 0);

	if(ret <= 0)
		print_error();

	return ret;
}

bool plain_socket::send(const char* buf, size_t len)
{
	size_t pos = 0;
	size_t slen = len;
	while(pos != slen)
	{
		int ret = ::send(hSocket, buf + pos, slen - pos, 0);
		if(ret == SOCKET_ERROR)
		{
			print_error();
			return false;
		}
		else
			pos += ret;
	}

	return true;
}

void plain_socket::close(bool free)
{
	if(hSocket == INVALID_SOCKET)
		return;
	
	if(free)
	{
		sock_closed = true;
		sock_close(hSocket);
		hSocket = INVALID_SOCKET;
	}
	else
	{
		sock_shutdown(hSocket);
	}
}

void plain_socket::print_error()
{
	char buff[512];
	const char* error = sock_strerror(buff, sizeof(buff));

	executor::inst().log_error(error);
	printer::inst().print(out_colours::K_RED, "Socket error: ", error);
}

void plain_socket::print_error(int gai_err)
{
	char buff[512];
	const char* error = sock_gai_strerror(gai_err, buff, sizeof(buff));

	executor::inst().log_error(error);
	printer::inst().print(out_colours::K_RED, "Socket error: ", error);
}

#ifndef CONF_NO_TLS

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

void tls_socket::print_error()
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

	executor::inst().log_error(out);
	printer::inst().print(out_colours::K_RED, "Socket error: ", out);
	BIO_free(err_bio);
}

void tls_socket::init_ctx()
{
	const SSL_METHOD* method = SSLv23_method();

	if(method == nullptr)
		return;

	ctx = SSL_CTX_new(method);
	if(ctx == nullptr)
		return;

	SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1);
}

bool tls_socket::set_hostname(const char* sAddr)
{
	sock_closed = false;
	if(ctx == nullptr)
	{
		init_ctx();
		if(ctx == nullptr)
		{
			print_error();
			return false;
		}
	}

	if((bio = BIO_new_ssl_connect(ctx)) == nullptr)
	{
		print_error();
		return false;
	}

	int flag = 1;
	/* If it fails, it fails, we won't loose too much sleep over it */
	setsockopt(BIO_get_fd(bio, nullptr), IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(int));

	if(BIO_set_conn_hostname(bio, sAddr) != 1)
	{
		print_error();
		return false;
	}

	BIO_get_ssl(bio, &ssl);
	if(ssl == nullptr)
	{
		print_error();
		return false;
	}

	if(SSL_set_cipher_list(ssl, "HIGH:!aNULL:!PSK:!SRP:!MD5:!RC4:!SHA1") != 1)
	{
		print_error();
		return false;
	}
	return true;
}

bool tls_socket::connect()
{
	sock_closed = false;
	if(BIO_do_connect(bio) != 1)
	{
		print_error();
		return false;
	}

	if(BIO_do_handshake(bio) != 1)
	{
		print_error();
		return false;
	}

	/* Step 1: verify a server certificate was presented during the negotiation */
	X509* cert = SSL_get_peer_certificate(ssl);
	if(cert == nullptr)
	{
		print_error();
		return false;
	}

	const EVP_MD* digest;
	unsigned char md[EVP_MAX_MD_SIZE];
	unsigned int dlen;

	digest = EVP_get_digestbyname("sha256");
	if(digest == nullptr)
	{
		print_error();
		return false;
	}

	if(X509_digest(cert, digest, md, &dlen) != 1 || dlen != 32)
	{
		X509_free(cert);
		print_error();
		return false;
	}

	//Base64 encode digest
	BIO *bmem, *b64;
	b64 = BIO_new(BIO_f_base64());
	bmem = BIO_new(BIO_s_mem());

	BIO_puts(bmem, "SHA256:");
	b64 = BIO_push(b64, bmem);
	BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
	BIO_write(b64, md, dlen);
	BIO_flush(b64);

	const char* conf_md = "";
	char* b64_md = nullptr;
	size_t b64_len = BIO_get_mem_data(bmem, &b64_md);

	char buffer[256];
	if(strlen(conf_md) == 0)
	{
		snprintf(buffer, sizeof(buffer),  "TLS fingerprint [%s] %.*s", BIO_get_conn_hostname(bio), (int)b64_len, b64_md);
		printer::inst().print(out_colours::K_YELLOW, buffer);
	}
	else if(strncmp(b64_md, conf_md, b64_len) != 0)
	{
		snprintf(buffer, sizeof(buffer), "FINGERPRINT FAILED CHECK [%s] %.*s was given, %s was configured",
				 BIO_get_conn_hostname(bio), (int)b64_len, b64_md, conf_md);
		executor::inst().log_error(buffer);
		printer::inst().print(out_colours::K_RED, "Socket error: ", buffer);
		BIO_free_all(b64);
		X509_free(cert);
		return false;
	}

	BIO_free_all(b64);
	X509_free(cert);
	return true;
}

int tls_socket::recv(char* buf, unsigned int len)
{
	if(sock_closed)
		return 0;

	int ret = BIO_read(bio, buf, len);

	if(ret == 0)
		print_error_s("RECEIVE error: socket closed");
	if(ret < 0)
		print_error();

	return ret;
}

bool tls_socket::send(const char* buf, size_t len)
{
	if(BIO_write(bio, buf, len) > 0)
		return true;
	
	print_error();
	return false;
}

void tls_socket::close(bool free)
{
	if(bio == nullptr || ssl == nullptr)
		return;

	sock_closed = true;
	if(!free)
	{
		sock_shutdown(BIO_get_fd(bio, nullptr));
	}
	else
	{
		BIO_free_all(bio);
		ssl = nullptr;
		bio = nullptr;
	}
}
#endif
