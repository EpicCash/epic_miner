#pragma once

#include "uv.hpp"
#include "vector32.hpp"
#include <thread>

#include "randomx/src/intrin_portable.h"
#include "randomx/src/dataset.hpp"
#include "randomx/src/vm_compiled.hpp"
#include "randomx/src/blake2/blake2.h"

#include "console/console.hpp"

constexpr uint64_t DatasetSize = RANDOMX_DATASET_BASE_SIZE + RANDOMX_DATASET_EXTRA_SIZE;

struct dataset
{
	dataset()
	{
		ready = false;
		id = 0;

		if(!try_alloc_dataset((randomx_flags)(RANDOMX_FLAG_LARGE_PAGES | RANDOMX_FLAG_JIT)))
		{
			if(!try_alloc_dataset((randomx_flags)RANDOMX_FLAG_JIT))
			{
				printer::inst().print(out_colours::K_RED, "Failed to allocate RandomX dataset (not enough RAM).");
				exit(0);
			}
		}
	}

	dataset(const dataset&) = delete;
	dataset(dataset&&) = delete;
	dataset& operator=(const dataset&) = delete;
	dataset& operator=(dataset&&) = delete;

	inline bool try_alloc_dataset(randomx_flags fl)
	{
		ds = randomx_alloc_dataset(fl);
		if(ds == nullptr)
			return false;
		ch = randomx_alloc_cache(fl);
		if(ch == nullptr)
		{
			randomx_release_dataset(ds);
			return false;
		}
		return true;
	}

	~dataset()
	{
		randomx_release_dataset(ds);
		randomx_release_cache(ch);
	}

	void calculate_dataset(const v32& seed);
	uint64_t get_dataset_id() { return seed.get_id(); }
	bool is_dataset_ready(uint64_t id) { return seed.get_id() == id && ready; }

	randomx_dataset* get_dataset() { return ds; }

private:
	void calculate_thd(size_t idx, size_t thd_cnt);
	void dataset_thd_main();

	randomx_cache* ch;
	randomx_dataset* ds;
	v32 seed;
	uint64_t id;
	bool ready;

	std::thread dscalc;
	uv_async_t async;
};
