#include "randomx/src/intrin_portable.h"
#include "randomx/src/dataset.hpp"
#include "randomx/src/vm_compiled.hpp"
#include "randomx/src/blake2/blake2.h"

constexpr uint64_t DatasetSize = RANDOMX_DATASET_BASE_SIZE + RANDOMX_DATASET_EXTRA_SIZE;

struct dataset
{
	dataset()
	{
		ch = randomx_alloc_cache((randomx_flags)(RANDOMX_FLAG_JIT));
		ds = randomx_alloc_dataset((randomx_flags)(0));
	}

	~dataset()
	{
		randomx_release_dataset(ds);
		randomx_release_cache(ch);
	}

	void init(const uint8_t* seed, size_t key_size)
	{
		randomx_init_cache(ch, seed, key_size);
	}

	uint64_t get_dataset_hash(size_t i) const
	{
		uint64_t hash = 0;
		const uint64_t* pds = reinterpret_cast<const uint64_t*>(randomx_get_dataset_memory(ds));
		for(size_t i=0; i < DatasetSize/sizeof(uint64_t); i++)
			hash ^= pds[i];
		return hash;
	}

	void calculate(size_t idx, size_t thd_cnt)
	{
		size_t work_size = randomx_dataset_item_count();
		size_t batch_size = work_size / thd_cnt;
		randomx_init_dataset(ds, ch, idx*batch_size, idx == thd_cnt-1 ? (batch_size+work_size%thd_cnt) : batch_size);
	}

	randomx_cache* ch;
	randomx_dataset* ds; 
};
