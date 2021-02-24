#pragma once

#include <inttypes.h>
#include <atomic>

enum class pow_type : uint32_t
{
	idle,
	randomx,
	progpow,
	cuckoo
};

struct pool_job
{
	pool_job() { reset(); }
	void reset() { nonce = 0; blob_len = 0; }
	bool is_active() { return blob_len > 0; }

	char jobid[64];
	uint8_t blob[512];
	uint8_t randomx_seed[32];
	uint32_t blob_len;
	uint32_t target;
	std::atomic<uint32_t> nonce;
	pow_type type;
};

struct miner_job
{
	miner_job()
	{
		type = pow_type::idle;
		nonce = nullptr;
		blob_len = 0;
		target = 0;
	}

	miner_job(pool_job& job)
	{
		memcpy(jobid, job.jobid, sizeof(jobid));
		memcpy(blob, job.blob, sizeof(blob));
		memcpy(randomx_seed, job.randomx_seed, sizeof(randomx_seed));
		blob_len = job.blob_len;
		target = job.target;
		type = job.type;
		nonce = &job.nonce;
	}

	char jobid[64];
	uint8_t blob[512];
	uint8_t randomx_seed[32]; // TODO: replace with central dataset
	uint32_t blob_len;
	uint32_t target;
	std::atomic<uint32_t>* nonce;
	pow_type type;
};
