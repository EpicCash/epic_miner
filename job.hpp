#pragma once

#include <inttypes.h>
#include <atomic>

enum class pow_type : uint32_t
{
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
