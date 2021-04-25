#pragma once

#include <inttypes.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <list>

#include "uv.hpp"
#include "job.hpp"
#include "misc.hpp"

class executor;

struct hash_rec
{
	hash_rec() : time(0), hashes(0) {}
	hash_rec(int64_t time, uint64_t hashes) : time(time), hashes(hashes) {}
	// if the time value are negative it means we are on idle boundary
	// time between two negative time values should not be counted towards h/r
	int64_t time;
	int64_t hashes;
};

typedef ring_buffer<hash_rec, 8192> hashlog_t;

class miner
{
public:
	miner(size_t thd_id);
	virtual ~miner() { main_thread.join(); }

	void stop_miner() { run = false; }

	void record_hashes(int64_t time_now) 
	{
		bool hashing = is_hashing.load(std::memory_order_relaxed);
		int64_t cnt = hash_count.load(std::memory_order_relaxed);
		if(!hashing)
			time_now = -time_now;
		hash_log.insert(hash_rec(time_now, cnt));
	}

	const hashlog_t& get_hash_log()
	{
		return hash_log;
	}

	bool dataset_check_loop();

protected:
	virtual void thread_main() = 0;
	void idle_loop();

	size_t thd_id;
	std::atomic<bool> run;
	miner_job* last_job = nullptr;
	miner_job current_job;

	std::mutex queue_mtx;
	std::list<result> result_q;

	uv_async_t async;
	executor& exec;

	std::atomic<bool> is_hashing;
	std::atomic<int64_t> hash_count;
	hashlog_t hash_log;

	std::thread main_thread;
	static constexpr uint32_t nonce_chunk = 16384;
};
