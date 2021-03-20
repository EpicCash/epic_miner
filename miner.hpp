#pragma once

#include <inttypes.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <list>

#include "job.hpp"
#include "misc.hpp"

class executor;

struct hash_rec
{
	hash_rec() : time(0), hashes(0) {}
	hash_rec(int64_t time, uint64_t hashes) : time(time), hashes(hashes) {}
	int64_t time;
	uint64_t hashes;
};

typedef ring_buffer<hash_rec, 8192> hashlog_t;

class miner
{
public:
	miner(size_t thd_id);
	~miner() { main_thread.join(); }

	void stop_miner() { run = false; }

	void record_hashes(int64_t time_now) 
	{ 
		uint64_t cnt = hash_count.load(std::memory_order_relaxed);
		hash_log.insert(hash_rec(time_now, cnt));
	}

	const hashlog_t& get_hash_log()
	{
		return hash_log;
	}

private:
	void thread_main();

	void idle_loop();
	void randomx_loop();

	size_t thd_id;
	std::atomic<bool> run;
	miner_job* last_job = nullptr;
	miner_job current_job;

	std::mutex queue_mtx;
	std::list<result> result_q;

	uv_async_t async;
	executor& exec;

	std::atomic<uint64_t> hash_count;
	hashlog_t hash_log;

	std::thread main_thread;
	static constexpr uint32_t nonce_chunk = 16384;
};
