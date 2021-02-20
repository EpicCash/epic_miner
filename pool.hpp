#pragma once

#include <memory>

#include "net/net_interface.hpp"
#include "net/tls_interface.hpp"

class pool
{
public:
	pool() {}

	bool init_pool(const char* hostname, bool use_tls)
	{
		using namespace std::placeholders;
		tls = use_tls;
		if(use_tls)
		{
			net = std::make_unique<tls_interface>(std::bind(&pool::net_on_connect, this, _1), 
												  std::bind(&pool::net_on_data_recv, this, _1, _2),
												  std::bind(&pool::net_on_error, this, _1),
												  std::bind(&pool::net_on_close, this));
		}
		else
		{
			net = std::make_unique<net_interface>(std::bind(&pool::net_on_connect, this, _1), 
												  std::bind(&pool::net_on_data_recv, this, _1, _2),
												  std::bind(&pool::net_on_error, this, _1),
												  std::bind(&pool::net_on_close, this));
		}

		return net->set_hostname(hostname);
	}

private:
	void net_on_connect(const char* tls_fp)
	{
	}

	ssize_t net_on_data_recv(const char* data, size_t len)
	{
		return len;
	}

	void net_on_error(const char* error)
	{
	}

	void net_on_close()
	{
	}

	std::unique_ptr<net_interface> net;
	bool tls;
};
