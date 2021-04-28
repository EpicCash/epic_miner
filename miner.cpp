#include "miner.hpp"
#include "executor.hpp"

miner::miner(uint32_t thd_id, miner_type type) :
 	thd_id(thd_id), type(type), run(true), exec(executor::inst()), is_hashing(false), hash_count(0)
{
	async.data = this;
	uv_async_init(uv_loop, &async, [](uv_async_t* handle) {
		miner* ths = reinterpret_cast<miner*>(handle->data);
		std::unique_lock<std::mutex> lock(ths->queue_mtx);
		while(!ths->result_q.empty())
		{
			miner_result res = ths->result_q.front();
			ths->result_q.pop_front();
			lock.unlock();
			executor::inst().on_found_result(res);
			lock.lock();
		}
	});

	hash_log.fill(hash_rec());
	// lambda wrapper is needed for some compilers since thread_main is pure virtual
	main_thread = std::thread([](miner* ths){ ths->thread_main(); }, this);
}

bool miner::dataset_check_loop()
{
	while(current_job.ds->ready_id != current_job.dataset_id)
	{
		if(exec.get_current_job() != last_job)
			return false;
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	return true;
}

void miner::idle_loop()
{
	while(exec.get_current_job() == last_job)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}
