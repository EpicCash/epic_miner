#pragma once

#include <thread>

#include "ethash/keccak.hpp"
#include "ethash/progpow.hpp"

#include "dataset.hpp"
#include "vector32.hpp"


struct pp_dataset : public dataset
{
	pp_dataset() : dataset(), dataset_ptr(nullptr)
	{
	}

	pp_dataset(const pp_dataset&) = delete;
	pp_dataset(pp_dataset&&) = delete;
	pp_dataset& operator=(const pp_dataset&) = delete;
	pp_dataset& operator=(pp_dataset&&) = delete;

	~pp_dataset()
	{
		if(dataset_ptr != nullptr)
			ethash_destroy_epoch_context_full(dataset_ptr);
	}

	inline void adjust_to_block(uint64_t block_number)
	{
		uint64_t epoch_num = ethash::get_epoch_number(block_number);
		if(loaded_id != epoch_num)
			calculate_dataset(epoch_num);
	}

	inline ethash::epoch_context_full* get_dataset() { return dataset_ptr; }
private:
	void calculate_dataset(uint64_t epoch_num);
	void dataset_thd_main(uint64_t epoch_num);

	ethash::epoch_context_full* dataset_ptr;
};

