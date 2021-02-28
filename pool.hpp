#pragma once

#include <memory>

#include "misc.hpp"
#include "json.hpp"
#include "job.hpp"
#include "net/net_interface.hpp"
#include "net/tls_interface.hpp"
#include <stdio.h>

class pool
{
public:
	pool(uint32_t pool_id);

	bool init_pool(const char* hostname, bool use_tls, const char* username, const char* password)
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

		if(!net->set_hostname(hostname))
			return false;

		this->username = username;
		this->password = password;
		return true;
	}

	void do_connect()
	{
		flush_call_map();
		net->do_connect();
	}

	void check_call_timeouts()
	{
		int64_t ts = get_timestamp_ms();
		for(const call_mapping& mp : call_map)
		{
			if(mp.type != call_types::invalid && ts - mp.time_made > 10000)
				return;
		}
	}

	pool_job& get_pool_job()
	{
		return my_job;
	}

	void do_send_result(const result& res);

private:
	enum class call_types : uint32_t
	{
		invalid,
		login,
		result,
		keepalive
	};

	void net_on_connect(const char* tls_fp);
	ssize_t net_on_data_recv(char* data, size_t len);
	void net_on_error(const char* error);
	void net_on_close();

	void do_login_call();

	bool process_json_doc();
	bool process_pool_job(const Value& param);
	bool process_pool_login(lpcJsVal value, const char* err_msg);
	bool process_pool_result(lpcJsVal value, const char* err_msg);
	bool process_pool_keepalive(lpcJsVal, const char* err_msg);

	bool protocol_error(const char* err)
	{
		printf("error: %s\n", err);
		return false;
	}

	uint32_t pool_id;

	std::unique_ptr<net_interface> net;
	bool tls;

	std::string pool_miner_id;
	std::string username;
	std::string password;
	std::string rigid;

	constexpr static size_t json_buffer_len = 4 * 1024;
	char json_parse_buf[json_buffer_len];
	char json_dom_buf[json_buffer_len];
	MemoryPoolAllocator<> domAlloc;
	MemoryPoolAllocator<> parseAlloc;
	MemDocument jsonDoc;

	pool_job my_job;

	struct call_mapping
	{
		uint32_t call_id;
		call_types type;
		int64_t time_made;
	};

	uint32_t g_call_id;
	call_mapping call_map[8];

	void flush_call_map()
	{
		g_call_id = 0;
		for(call_mapping& mp : call_map)
			mp.type = call_types::invalid;
	}

	call_types find_call_type(uint32_t call_id)
	{
		for(call_mapping& mp : call_map)
		{
			if(mp.type != call_types::invalid && mp.call_id == call_id)
			{
				call_types ret = mp.type;
				mp.type = call_types::invalid;
				return ret;
			}
		}
		return call_types::invalid;
	}

	bool register_call(call_types type, uint32_t& call_id)
	{
		for(call_mapping& mp : call_map)
		{
			if(mp.type == call_types::invalid)
			{
				mp.type = type;
				mp.time_made = get_timestamp_ms();
				call_id = g_call_id;
				mp.call_id = g_call_id;
				g_call_id++;
				return true;
			}
		}
		return false;
	}
};
