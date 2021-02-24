#pragma once
#include <list>
#include <functional>
#include <unordered_map>

#include "dataset.hpp"
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

	inline miner_job* get_current_job()
	{
		return current_job.load(std::memory_order_relaxed);
	}

private:
	executor() {}
	void close();

	void on_heartbeat();

	void push_idle_job()
	{
		miner_jobs.emplace_front();
		current_job = &miner_jobs.front();
	}

	struct error_info
	{
		int64_t last_seen;
		size_t count=1;
	};

	std::unordered_map<std::string, error_info> error_log;

	std::vector<std::unique_ptr<pool>> pools;

	std::vector<dataset> randomx_datasets;
	// miner_job is immutable, once the atomic pointer is set
	// the threads can copy it without locks
	std::list<miner_job> miner_jobs;
	std::atomic<miner_job*> current_job;

	input_console in_console;

	uv_timer_t heartbeat_timer;
};
