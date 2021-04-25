#include "rx_miner.hpp"
#include "executor.hpp"

void rx_cpu_miner::randomx_loop()
{
	rx_dataset& ds = *reinterpret_cast<rx_dataset*>(current_job.ds);

	if(!dataset_check_loop())
		return;

	std::shared_lock<std::shared_timed_mutex> lk(ds.mtx);
	randomx_flags fl = (randomx_flags)(RANDOMX_FLAG_FULL_MEM | RANDOMX_FLAG_JIT | RANDOMX_FLAG_HARD_AES);
	randomx_vm* v = randomx_create_vm(fl, nullptr, ds.get_dataset());
	uint32_t* nonce_ptr = reinterpret_cast<uint32_t*>(current_job.blob + current_job.nonce_pos);
	*nonce_ptr = 0;
	v32 job_hash;
	
	is_hashing = true;
	while(exec.get_current_job() == last_job)
	{
		if(*nonce_ptr % nonce_chunk == 0)
			*nonce_ptr = current_job.nonce->fetch_add(nonce_chunk, std::memory_order_relaxed);
		else
			(*nonce_ptr)++;
		
		randomx_calculate_hash(v, current_job.blob, current_job.blob_len, job_hash.data);
		if(job_hash.get_work32() < current_job.target)
		{
			std::unique_lock<std::mutex> lock(queue_mtx);
			result_q.emplace_back(current_job.id, nonce_ptr[0], job_hash);
			lock.unlock();
			uv_async_send(&async);
		}
		
		hash_count.fetch_add(1, std::memory_order_relaxed);
	}
	is_hashing = false;
	
	randomx_destroy_vm(v);
}

void rx_cpu_miner::thread_main()
{
	while(run.load(std::memory_order_relaxed))
	{
		last_job = exec.get_current_job();
		current_job = *last_job;

		if(current_job.type == pow_type::randomx)
			randomx_loop();
		else
			idle_loop();
	}
}

