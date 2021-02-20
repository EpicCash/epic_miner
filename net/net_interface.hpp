#pragma once 

#include <uv.h>
#include <assert.h>
#include <functional>
#include <string>

extern uv_loop_t* uv_loop;

class net_interface
{
public:
	typedef std::function<void(const char* tls_fingerprint)> on_connect_t;
	// called with all available data, returns size of data consumed, or -1
	// 0 is a valid answer - "call me with all of that data when you have some more"
	typedef std::function<ssize_t(char* data, size_t len)> data_consumer_t; 
	typedef std::function<void(const char* data)> on_error_t;
	typedef std::function<void()> on_close_t;

	net_interface(on_connect_t&& on_connect, data_consumer_t&& on_recv_data, on_error_t&& on_error, on_close_t&& on_close);

	bool set_hostname(const char* ip_port_str);
	void do_connect();

	virtual void do_socket_write(const char* data, int len);
	char* get_send_buffer(uint32_t& max_len, uint32_t& idx);
	void finalize_socket_write(uint32_t idx, uint32_t len);

	virtual void do_shutdown();

protected:
	static void on_dns_resolved_st(uv_getaddrinfo_t* r_resolv, int status, addrinfo* res);
	void on_dns_resolved(addrinfo* res);
	void on_dns_error(int error);

	static void on_connnect_st(uv_connect_t* r_conn, int status);
	virtual void on_connect();
	void on_connect_error(int error);

	static void alloc_recv_buffer(uv_handle_t* h_sock, size_t, uv_buf_t* buf);
	static void on_socket_read_st(uv_stream_t* h_sock, ssize_t nread, const uv_buf_t* buf);
	static bool consume_loop(const data_consumer_t& consume, char* buffer, uint32_t& recv_pos);
	virtual bool on_socket_read();

	static void on_socket_write_st(uv_write_t* r_send, int status);
	void on_socket_write_complete(uint32_t idx);

	void on_socket_error(int error);

	static void on_shutdown_st(uv_shutdown_t* r_shutdown, int);
	inline void do_close_socket();
	static void on_socket_close_st(uv_handle_t* h_sock);
	virtual void on_socket_closed();

protected:
	uv_tcp_t h_sock;
	uv_connect_t r_conn;
	uv_getaddrinfo_t r_resolv;
	uv_shutdown_t r_shutdown;
	
	struct send_st
	{
		uv_write_t r_send = {0};
		char data[2048];
		uint32_t data_len;
		bool active;
	};

	std::string host;
	std::string port;

	on_connect_t notify_connect;
	data_consumer_t consume_data;
	on_error_t notify_error;
	on_close_t notify_close;
	
	send_st send_bufs[4];
	char recv_buf[4096];
	uint32_t recv_pos = 0;
	bool shutting_down = false;
};
