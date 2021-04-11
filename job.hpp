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

inline const char* pow_type_to_str(pow_type type)
{
	switch(type)
	{
		case pow_type::randomx:
			return "randomx";
		case pow_type::progpow:
			return "progpow";
		case pow_type::cuckoo:
			return "cuckoo";
		case pow_type::idle:
			return "idle";
	}
}

struct jobid
{
	char data[64];
};

struct pool_job
{
	pool_job() { reset(); }
	void reset() { nonce = 0; blob_len = 0; }
	bool is_active() { return blob_len > 0; }

	jobid id;
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

	miner_job(pool_job& job, dataset* ds) : id(job.id), ds(ds)
	{
		memcpy(blob, job.blob, sizeof(blob));
		blob_len = job.blob_len;
		nonce_pos = job.nonce_pos;
		target = job.target;
		type = job.type;
		nonce = &job.nonce;
	}

	jobid id;
	uint8_t blob[512];
	uint32_t blob_len;
	uint32_t nonce_pos;
	uint32_t target;
	dataset* ds;
	std::atomic<uint32_t>* nonce;
	pow_type type;
};

struct result
{
	result(const jobid& id, uint32_t nonce, const v32& res_hash) : id(id), nonce(nonce), res_hash(res_hash) {}
	jobid id;
	uint32_t nonce;
	v32 res_hash;
};
