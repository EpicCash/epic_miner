#include "miner.hpp"
#include "executor.hpp"

miner::miner(size_t thd_id) :
	thd_id(thd_id), main_thread(&miner::thread_main, this), run(true), exec(executor::inst())
{
}

void miner::idle_loop()
{
	while(exec.get_current_job() == last_job)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}

void miner::randomx_loop()
{
	while(exec.get_current_job() == last_job)
	{
		
	}
}

void miner::thread_main()
{
	while(run.load(std::memory_order_relaxed))
	{
		last_job = exec.get_current_job();
		current_job = *last_job;

		switch(current_job.type)
		{
		case pow_type::idle:
			idle_loop();
			break;
		case pow_type::randomx;
			randomx_loop();
			break;
		case pow_type::progpow:
			idle_loop();
			break;
		case pow_type::cuckoo:
			idle_loop();
			break;
		}
	}
}
