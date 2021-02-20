#include "net_interface.hpp"
#include <string.h>

net_interface::net_interface(on_connect_t&& on_connect, data_consumer_t&& on_recv_data, on_error_t&& on_error, on_close_t&& on_close) : 
	h_sock{0}, r_conn{0}, r_resolv{0}, r_shutdown{0}, notify_connect(std::move(on_connect)), 
	consume_data(std::move(on_recv_data)), notify_error(std::move(on_error)), notify_close(std::move(on_close))
{
}

net_interface::~net_interface()
{
}

bool net_interface::set_hostname(const char* ip_port_str)
{
	char ip_port[256];
	int len = strlen(ip_port_str);

	if(len >= sizeof(ip_port))
	{
		notify_error("Hostname too long");
		return false;
	}

	memcpy(ip_port, ip_port_str, len);
	ip_port[len] = '\0';

	char* cl = strrchr(ip_port, ':');
	if(cl == nullptr)
	{
		notify_error("Pool address format is hostname:port");
		return false;
	}

	char* port = cl+1;
	cl[0] = '\0';
	char* host = ip_port;
	len = strlen(host);

	if(host[0] == '[' && host[len-1] == ']')
	{
		// IPv6 style address -> [::ffff]:1234
		host++;
		host[len-1] = '\0';
	}

	this->host = host;
	this->port = port;
	return true;
}

void net_interface::do_connect()
{
	addrinfo hints = {0};
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	assert(r_resolv.data == nullptr);
	r_resolv.data = reinterpret_cast<void*>(this);

	uv_getaddrinfo(uv_loop, &r_resolv, on_dns_resolved_st, host.c_str(), port.c_str(), &hints);
}

void net_interface::on_dns_resolved_st(uv_getaddrinfo_t* r_resolv, int status, addrinfo* res)
{
	if(status == 0)
		reinterpret_cast<net_interface*>(r_resolv->data)->on_dns_resolved(res);
	else
		reinterpret_cast<net_interface*>(r_resolv->data)->on_dns_error(status);
	*r_resolv = {0};
}

void net_interface::on_dns_resolved(addrinfo* res)
{
	addrinfo* ptr = res;
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

	addrinfo* final_addr;
	if(ipv4.empty() && ipv6.empty())
	{
		notify_error("DNS Error: No IPv4 or IPv6 records");
		return;
	}
	else if(!ipv4.empty() && ipv6.empty())
		final_addr = ipv4[rand() % ipv4.size()];
	else if(ipv4.empty() && !ipv6.empty())
		final_addr = ipv6[rand() % ipv6.size()];
	else
	{
		if( (true) )
			final_addr = ipv4[rand() % ipv4.size()];
		else
			final_addr = ipv6[rand() % ipv6.size()];
	}

	assert(h_sock.data == nullptr);
	uv_tcp_init(uv_loop, &h_sock);
	h_sock.data = reinterpret_cast<void*>(this);

	assert(r_conn.data == nullptr);
	r_conn.data = reinterpret_cast<void*>(this);
	uv_tcp_connect(&r_conn, &h_sock, final_addr->ai_addr, on_connnect_st);
	uv_freeaddrinfo(res);
}

void net_interface::on_dns_error(int error)
{
	char buf[64];
	uv_strerror_r(error, buf, sizeof(buf));
	notify_error(buf);
}

void net_interface::on_connnect_st(uv_connect_t* r_conn, int status)
{
	if(status == 0)
		reinterpret_cast<net_interface*>(r_conn->data)->on_connect();
	else
		reinterpret_cast<net_interface*>(r_conn->data)->on_connect_error(status);
	*r_conn = {0};
}

void net_interface::on_connect()
{
	notify_connect(nullptr);
	uv_read_start(reinterpret_cast<uv_stream_t*>(&h_sock), alloc_recv_buffer, on_socket_read_st);
}

void net_interface::on_connect_error(int error)
{
	char buf[64];
	uv_strerror_r(error, buf, sizeof(buf));
	notify_error(buf);
}

void net_interface::alloc_recv_buffer(uv_handle_t* h_sock, size_t, uv_buf_t* buf) 
{
	net_interface* ths = reinterpret_cast<net_interface*>(h_sock->data);
	buf->base = ths->recv_buf + ths->recv_pos;
	buf->len = sizeof(ths->recv_buf) - ths->recv_pos;
}

void net_interface::on_socket_read_st(uv_stream_t* h_sock, ssize_t nread, const uv_buf_t* buf)
{
	if(nread > 0)
	{
		net_interface* ths = reinterpret_cast<net_interface*>(h_sock->data);
		ths->recv_pos += nread;
		if(!ths->on_socket_read())
			ths->do_shutdown();
	}
	if(nread < 0)
		reinterpret_cast<net_interface*>(h_sock->data)->on_socket_error(nread);
	// nread == 0 is a non-event but valid apparently (EAGAIN)
}

bool net_interface::consume_loop(const data_consumer_t& consume, char* buffer, uint32_t& recv_pos)
{
	size_t consumed = 0;
	while(true)
	{
		ssize_t cd = consume(buffer+consumed, recv_pos-consumed);
		assert(cd <= recv_pos-consumed);
		if(cd < 0)
			return false;
		consumed += cd;
		// Break if all data has been consumed, or consume_data didn't consume anything
		if(cd == 0 || recv_pos == consumed)
			break;
	}
	
	if(recv_pos > consumed && consumed > 0)
		memmove(buffer, buffer+consumed, recv_pos-consumed);
	recv_pos -= consumed;
	return true;
}

bool net_interface::on_socket_read() 
{
	return consume_loop(consume_data, recv_buf, recv_pos);
}

void net_interface::do_socket_write(const char* data, int len)
{
	uint32_t idx;
	uint32_t buflen;
	char* buffer = get_send_buffer(buflen, idx);
	if(buffer == nullptr)
	{
		notify_error("Limit of 4 concurrent sends exceeded!");
		do_shutdown();
		return;
	}
	
	if(len > buflen)
	{
		notify_error("Buffer overflow on send!");
		do_shutdown();
		return;
	}
	
	memcpy(buffer, data, len);
	finalize_socket_write(idx, len);
}

char* net_interface::get_send_buffer(uint32_t& max_len, uint32_t& idx)
{
	uint32_t i=0;
	for(; i < std::size(send_bufs); i++)
	{
		if(!send_bufs[i].active)
			break;
	}
	
	if(i == std::size(send_bufs))
		return nullptr;
	
	send_bufs[i].active = true;
	max_len = sizeof(send_bufs[i].data);
	idx = i;
	return send_bufs[i].data;
}

void net_interface::finalize_socket_write(uint32_t idx, uint32_t len)
{
	assert(idx < std::size(send_bufs) && send_bufs[idx].active);
	
	send_st& st = send_bufs[idx];
	if(len == 0)
	{
		st.active = false;
		return;
	}
	
	uv_buf_t wrbuf;
	wrbuf.base = st.data;
	wrbuf.len = len;
	
	assert(st.r_send.data == nullptr);
	st.r_send.data = this;
	uv_write(&st.r_send, reinterpret_cast<uv_stream_t*>(&h_sock), &wrbuf, 1, on_socket_write_st);
}

void net_interface::on_socket_write_st(uv_write_t* r_send, int status)
{
	net_interface* ths = reinterpret_cast<net_interface*>(r_send->data);
	if(status == 0)
	{
		// Bit of pointer magic to figure out idx
		char* base = reinterpret_cast<char*>(&ths->send_bufs[0].r_send);
		char* us = reinterpret_cast<char*>(r_send);
		assert(base <= us);
		uint64_t idx = static_cast<uint32_t>(us-base);
		assert(idx % sizeof(send_st) == 0);
		idx /= sizeof(send_st);
		assert(idx < std::size(ths->send_bufs));
		
		ths->on_socket_write_complete(idx);
	}
	else
	{
		ths->on_socket_error(status);
	}
}

void net_interface::on_socket_write_complete(uint32_t idx)
{
	assert(idx < std::size(send_bufs) && send_bufs[idx].active);
	send_st& st = send_bufs[idx];
	st.r_send = {0};
	st.active = false;
}

void net_interface::on_socket_error(int error)
{
	if(error == UV_EOF && !shutting_down)
	{
		do_shutdown();
		return;
	}
	else
	{
		char buf[64];
		uv_strerror_r(error, buf, sizeof(buf));
		notify_error(buf);
		do_shutdown();
	}
}

void net_interface::do_shutdown()
{
	if(shutting_down)
		return;
	shutting_down = true;
	assert(r_shutdown.data == nullptr);
	r_shutdown.data = reinterpret_cast<void*>(this);
	uv_shutdown(&r_shutdown, reinterpret_cast<uv_stream_t*>(&h_sock), on_shutdown_st);
}

void net_interface::on_shutdown_st(uv_shutdown_t* r_shutdown, int)
{
	reinterpret_cast<net_interface*>(r_shutdown->data)->do_close_socket();
	*r_shutdown = {0};
}

inline void net_interface::do_close_socket()
{
	uv_close(reinterpret_cast<uv_handle_t*>(&h_sock), on_socket_close_st);
}

void net_interface::on_socket_close_st(uv_handle_t* h_sock)
{
	reinterpret_cast<net_interface*>(h_sock->data)->on_socket_closed();
}

void net_interface::on_socket_closed()
{
	h_sock = {0};
	recv_pos = 0;
	for(size_t i = 0; i < std::size(send_bufs); i++)
		send_bufs[i].active = false;
	shutting_down = false;
	notify_close();
}
