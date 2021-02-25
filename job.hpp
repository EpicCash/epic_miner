#pragma once

#include <inttypes.h>
#include <atomic>
#include "vector32.hpp"
#include "dataset.hpp"

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
	v32 randomx_seed;
	uint32_t blob_len;
	uint32_t nonce_pos;
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

	miner_job(pool_job& job, dataset* ds) : ds(ds)
	{
		memcpy(jobid, job.jobid, sizeof(jobid));
		memcpy(blob, job.blob, sizeof(blob));
		blob_len = job.blob_len;
		nonce_pos = job.nonce_pos;
		target = job.target;
		type = job.type;
		nonce = &job.nonce;
	}

	char jobid[64];
	uint8_t blob[512];
	uint32_t blob_len;
	uint32_t nonce_pos;
	uint32_t target;
	dataset* ds;
	std::atomic<uint32_t>* nonce;
	pow_type type;
};
