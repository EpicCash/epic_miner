#pragma once
#include <mutex>
#include <functional>
#include <unordered_map>

#include "exec_events.hpp"
#include "thdq.hpp"
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
		std::unique_lock<std::mutex> lck(error_log_mtx);
		auto res = error_log.emplace(std::piecewise_construct,
									 std::forward_as_tuple(std::move(error_msg)),
									 std::forward_as_tuple());
		if(!res.second)
			res.first->second.count++;
		
		res.first->second.last_seen = get_timestamp_s();
	}

	inline void log_error(const char* error_msg)
	{
		std::unique_lock<std::mutex> lck(error_log_mtx);
		auto res = error_log.emplace(std::piecewise_construct,
									 std::forward_as_tuple(error_msg),
									 std::forward_as_tuple());
		if(!res.second)
			res.first->second.count++;
		
		res.first->second.last_seen = get_timestamp_s();
	}

private:
	executor() {}

	void on_executor_event(const std::unique_ptr<exec_message>& event);

	thdq<std::unique_ptr<exec_message>> event_q;

	struct error_info
	{
		int64_t last_seen;
		size_t count=1;
	};
	
	std::mutex error_log_mtx;
	std::unordered_map<std::string, error_info> error_log;
};
