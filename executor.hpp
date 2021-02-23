#pragma once
#include <list>
#include <functional>
#include <unordered_map>

#include "pool.hpp"
#include "console/input_console.hpp"
#include "misc.hpp"

class executor
{
public:
	static executor& inst()
	{
		static executor inst;
		return inst;
	};

	void run();

	inline void log_error(std::string&& error_msg)
	{
		auto res = error_log.emplace(std::piecewise_construct,
									 std::forward_as_tuple(std::move(error_msg)),
									 std::forward_as_tuple());
		if(!res.second)
			res.first->second.count++;
		
		res.first->second.last_seen = get_timestamp_s();
	}

	inline void log_error(const char* error_msg)
	{
		auto res = error_log.emplace(std::piecewise_construct,
									 std::forward_as_tuple(error_msg),
									 std::forward_as_tuple());
		if(!res.second)
			res.first->second.count++;
		
		res.first->second.last_seen = get_timestamp_s();
	}

	void on_key_pressed(char key);
	void on_pool_new_job(uint32_t pool_id);

private:
	executor() {}
	void close();

	void on_heartbeat();

	struct error_info
	{
		int64_t last_seen;
		size_t count=1;
	};

	std::unordered_map<std::string, error_info> error_log;

	std::vector<std::unique_ptr<pool>> pools;

	input_console in_console;

	uv_timer_t heartbeat_timer;
};
