#pragma once

#include <inttypes.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <list>

#include "job.hpp"

class executor;

class miner
{
public:
	miner(size_t thd_id);
	~miner() { main_thread.join(); }

private:
	void thread_main();

	void idle_loop();
	void randomx_loop();

	size_t thd_id;
	std::thread main_thread;
	std::atomic<bool> run;
	miner_job* last_job = nullptr;
	miner_job current_job;

	std::mutex queue_mtx;
	std::list<result> result_q;

	uv_async_t async;
	executor& exec;
};
