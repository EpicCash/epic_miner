#pragma once

#include <memory>
#include <optional>

#include "misc.hpp"
#include "json.hpp"
#include "job.hpp"
#include "net/net_interface.hpp"
#include "net/tls_interface.hpp"
#include "console/console.hpp"
#include <stdio.h>

class pool
{
public:
	enum class pool_state : uint32_t
	{
		idle = 0,
		connecting = 1,
		connected = 2,
		has_job = 3
	};

	pool(uint32_t pool_id);

	bool init_pool(const char* hostname, bool use_tls, const char* tls_fp, const char* username, const char* password, const char* rigid)
	{
		using namespace std::placeholders;
#ifdef BUILD_NO_OPENSSL
		tls = false;
		net = std::make_unique<net_interface>(std::bind(&pool::net_on_connect, this, _1), 
											  std::bind(&pool::net_on_data_recv, this, _1, _2),
											  std::bind(&pool::net_on_error, this, _1),
											  std::bind(&pool::net_on_close, this));
#else
		tls = use_tls;
		if(use_tls)
		{
			net = std::make_unique<tls_interface>(std::bind(&pool::net_on_connect, this, _1), 
												  std::bind(&pool::net_on_data_recv, this, _1, _2),
												  std::bind(&pool::net_on_error, this, _1),
												  std::bind(&pool::net_on_close, this));
			this->tls_fp = tls_fp;
		}
		else
		{
			net = std::make_unique<net_interface>(std::bind(&pool::net_on_connect, this, _1), 
												  std::bind(&pool::net_on_data_recv, this, _1, _2),
												  std::bind(&pool::net_on_error, this, _1),
												  std::bind(&pool::net_on_close, this));
		}
#endif

		if(!net->set_hostname(hostname))
			return false;

		this->hostname = hostname;
		this->username = username;
		this->password = password;
		this->rigid = rigid;
		return true;
	}

	void do_connect()
	{
		printer::inst().print(K_BLUE, "Connecting to ", hostname.c_str());
		state = pool_state::connecting;
		flush_call_map();
		last_connect_attempt = get_timestamp_ms();
		net->do_connect();
	}

	void do_disconnect()
	{
		net->do_shutdown();
	}

	void check_call_timeouts()
	{
		int64_t ts = get_timestamp_ms();
		for(const call_mapping& mp : call_map)
		{
			if(mp.type != call_types::invalid && ts - mp.time_made > 10000)
			{
				net_on_error("call timeout");
				return;
			}
		}
	}

	pool_job& get_pool_job()
	{
		return my_job;
	}

	void do_send_result(const miner_result& res);

	pool_state get_pool_state()
	{
		return state;
	}

	int64_t get_last_connect_attempt()
	{
		return last_connect_attempt;
	}

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
	void net_on_error(const char* err);
	void net_on_close();

	void do_login_call();

	bool process_json_doc();
	bool process_pool_job(const Value& param);
	bool process_pool_login(lpcJsVal value, const char* err_msg);
	bool process_pool_keepalive(lpcJsVal, const char* err_msg);

	bool protocol_error(const char* err);

	uint32_t pool_id;
	pool_state state;

	std::unique_ptr<net_interface> net;
	bool tls;

	std::string hostname;
	std::string pool_miner_id;
	std::string username;
	std::string password;
	std::string rigid;
	std::string tls_fp;

	int64_t last_connect_attempt = 0;

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
		uint32_t miner_id;
	};

	uint32_t g_call_id;
	call_mapping call_map[8];

	uint32_t ping_call_id = -1;
	int64_t ping_timestamp = -1;

	void flush_call_map()
	{
		g_call_id = 0;
		for(call_mapping& mp : call_map)
			mp.type = call_types::invalid;
	}

	struct find_call_res
	{
		call_types type;
		uint32_t miner_id;
	};

	find_call_res find_call(uint32_t call_id)
	{
		for(call_mapping& mp : call_map)
		{
			if(mp.type != call_types::invalid && mp.call_id == call_id)
			{
				find_call_res ret = { mp.type, mp.miner_id };
				mp.type = call_types::invalid;
				mp.miner_id = invalid_miner_id;
				return ret;
			}
		}
		return { call_types::invalid, invalid_miner_id };
	}

	bool register_call(call_types type, uint32_t& call_id, uint32_t miner_id = invalid_miner_id)
	{
		for(call_mapping& mp : call_map)
		{
			if(mp.type == call_types::invalid)
			{
				mp.type = type;
				mp.time_made = get_timestamp_ms();
				call_id = g_call_id;
				mp.call_id = g_call_id;
				mp.miner_id = miner_id;
				g_call_id++;
				return true;
			}
		}
		return false;
	}
};
