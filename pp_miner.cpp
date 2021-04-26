#include "pp_miner.hpp"
#include "pp_dataset.hpp"
#include "executor.hpp"

inline uint32_t get_ethash_work32(const ethash::result& res)
{
	return __builtin_bswap32(*reinterpret_cast<const uint32_t*>(res.final_hash.bytes));
}

void pp_cpu_miner::progpow_loop()
{
	pp_dataset& ds = *reinterpret_cast<pp_dataset*>(current_job.ds);

	if(!dataset_check_loop())
		return;

	std::shared_lock<std::shared_timed_mutex> lk(ds.mtx);
	uint64_t nonce_base = *reinterpret_cast<uint32_t*>(current_job.blob + current_job.nonce_pos - sizeof(uint32_t));
	nonce_base <<= 32;
	uint32_t nonce_ctr = 0;

	ethash_hash256 header_hash = ethash::keccak256(current_job.blob, current_job.blob_len - sizeof(uint64_t));
	ethash::result job_hash;

	is_hashing = true;
	while(exec.get_current_job() == last_job)
	{
		if(nonce_ctr % nonce_chunk == 0)
			nonce_ctr = current_job.nonce->fetch_add(nonce_chunk, std::memory_order_relaxed);
		else
			nonce_ctr++;

		job_hash = progpow::hash(*ds.get_dataset(), current_job.height, header_hash, nonce_base | nonce_ctr);
		if(get_ethash_work32(job_hash) < current_job.target)
		{
			char hh[65] = {0};
			bin2hex(header_hash.bytes, 32, hh);
			printer::inst().print_dbg("Nonce : %llx %s", nonce_base | nonce_ctr, hh);
			std::unique_lock<std::mutex> lock(queue_mtx);
			result_q.emplace_back(current_job.id, nonce_ctr, v32(job_hash.final_hash.bytes));
			lock.unlock();
			uv_async_send(&async);
		}

		hash_count.fetch_add(1, std::memory_order_relaxed);
	}
	is_hashing = false;
}

void pp_cpu_miner::thread_main()
{
	while(run.load(std::memory_order_relaxed))
	{
		last_job = exec.get_current_job();
		current_job = *last_job;
		
		if(current_job.type == pow_type::progpow)
			progpow_loop();
		else
			idle_loop();
	}
}
