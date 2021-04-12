#pragma once
#ifndef BUILD_NO_OPENSSL
#include "net_interface.hpp"

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/conf.h>
#include <openssl/engine.h>

class tls_interface : public net_interface
{
public:
	tls_interface(on_connect_t&& on_connect, data_consumer_t&& on_recv_data, on_error_t&& on_error, on_close_t&& on_close);
	virtual ~tls_interface();

	virtual void do_socket_write(const char* data, int len) override;
	virtual void do_shutdown() override;

protected:
	// Note, those functions should not be used on tls interface, only do_socket_write
	using net_interface::get_send_buffer;
	using net_interface::finalize_socket_write;

	void get_tls_error();

	virtual void on_connect() override;
	void do_ssl_handshake();

	void flush_tls_pending();

	ssize_t consume_ct_data(const char* data, size_t len);

	virtual void on_socket_closed() override;

protected:
	BIO* ssl_bio = nullptr; // plaintext BIO
	BIO* app_bio = nullptr; // ciphertext BIO
	SSL* ssl = nullptr;

	bool handshake_complete;

	data_consumer_t consume_pt_data;

	char pt_buf[4096];
	uint32_t pt_pos = 0;
};
#endif
