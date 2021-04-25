#pragma once

#include <thread>

#include "dataset.hpp"
#include "uv.hpp"
#include "vector32.hpp"

#include "randomx/src/intrin_portable.h"
#include "randomx/src/dataset.hpp"
#include "randomx/src/vm_compiled.hpp"
#include "randomx/src/blake2/blake2.h"

#include "console/console.hpp"

constexpr uint64_t DatasetSize = RANDOMX_DATASET_BASE_SIZE + RANDOMX_DATASET_EXTRA_SIZE;

struct rx_dataset : public dataset
{
	rx_dataset() : dataset()
	{
		if(!try_alloc_dataset((randomx_flags)(RANDOMX_FLAG_LARGE_PAGES | RANDOMX_FLAG_JIT)))
		{
			if(!try_alloc_dataset((randomx_flags)RANDOMX_FLAG_JIT))
			{
				printer::inst().print(out_colours::K_RED, "Failed to allocate RandomX dataset (not enough RAM).");
				exit(0);
			}
		}
	}

	rx_dataset(const rx_dataset&) = delete;
	rx_dataset(rx_dataset&&) = delete;
	rx_dataset& operator=(const rx_dataset&) = delete;
	rx_dataset& operator=(rx_dataset&&) = delete;

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

	~rx_dataset()
	{
		randomx_release_dataset(ds);
		randomx_release_cache(ch);
	}

	inline void adjust_to_seed(const v32& seed)
	{
		if(loaded_id != seed.get_id())
			calculate_dataset(seed);
	}

	randomx_dataset* get_dataset() { return ds; }

private:
	void calculate_dataset(const v32& seed);
	void calculate_thd(size_t idx, size_t thd_cnt);
	void dataset_thd_main();

	randomx_cache* ch;
	randomx_dataset* ds;
	v32 seed;
};
