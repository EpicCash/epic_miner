#pragma once

#include "uv.hpp"
#include "vector32.hpp"
#include <thread>

#include "randomx/src/intrin_portable.h"
#include "randomx/src/dataset.hpp"
#include "randomx/src/vm_compiled.hpp"
#include "randomx/src/blake2/blake2.h"

constexpr uint64_t DatasetSize = RANDOMX_DATASET_BASE_SIZE + RANDOMX_DATASET_EXTRA_SIZE;

struct dataset
{
	dataset()
	{
		id = 0;
		ch = randomx_alloc_cache((randomx_flags)(RANDOMX_FLAG_JIT));
		ds = randomx_alloc_dataset((randomx_flags)(0));
	}

	dataset(const dataset&) = delete;
	dataset(dataset&&) = delete;
	dataset& operator=(const dataset&) = delete;
	dataset& operator=(dataset&&) = delete;

	~dataset()
	{
		randomx_release_dataset(ds);
		randomx_release_cache(ch);
	}

	uint64_t get_dataset_hash(size_t i) const
	{
		uint64_t hash = 0;
		const uint64_t* pds = reinterpret_cast<const uint64_t*>(randomx_get_dataset_memory(ds));
		for(size_t i=0; i < DatasetSize/sizeof(uint64_t); i++)
			hash ^= pds[i];
		return hash;
	}

	void calculate_dataset(const v32& seed);
	uint64_t get_dataset_id();

	randomx_dataset* get_dataset() { return ds; }

private:
	void calculate_thd(size_t idx, size_t thd_cnt);
	void dataset_thd_main();

	randomx_cache* ch;
	randomx_dataset* ds;
	v32 seed;
	uint64_t id;

	std::thread dscalc;
	uv_async_t async;
};
