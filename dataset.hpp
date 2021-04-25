#pragma once

#include <shared_mutex>
#include <atomic>

struct dataset
{
	static constexpr uint64_t invalid_id = uint64_t(-1);

	dataset() : loaded_id(invalid_id), ready_id(invalid_id) {}

	dataset(const dataset&) = delete;
	dataset(dataset&&) = delete;
	dataset& operator=(const dataset&) = delete;
	dataset& operator=(dataset&&) = delete;

	std::shared_timed_mutex mtx;
	std::atomic<uint64_t> loaded_id;
	std::atomic<uint64_t> ready_id;
};
