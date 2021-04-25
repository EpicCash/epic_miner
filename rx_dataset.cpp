#include "dataset.hpp"
#include "executor.hpp"

#include <shared_mutex>
#include <mutex>

void rx_dataset::calculate_dataset(const v32& seed)
{
	loaded_id = seed.get_id();
	ready_id = invalid_id;
	this->seed = seed;

	std::thread(&rx_dataset::dataset_thd_main, this).detach();
}

void rx_dataset::dataset_thd_main()
{
	std::lock_guard<std::shared_timed_mutex> lk(mtx);

	printer::inst().print(out_colours::K_MAGENTA, "Calculating RandomX dataset... Mining will resume soon...");
	printer::inst().print_dbg("Calculating dataset %zx!\n", seed.get_id());

	randomx_init_cache(ch, seed.data, 32);

	std::vector<std::thread> thd;
	size_t thdcnt = std::thread::hardware_concurrency();
	thd.reserve(thdcnt-1);

	for(size_t i=1; i < thdcnt; i++)
		thd.emplace_back(&rx_dataset::calculate_thd, this, i, thdcnt);
	
	calculate_thd(0, thdcnt);

	for(auto& t : thd)
		t.join();

	printer::inst().print_dbg("Dataset ready %zx!\n", seed.get_id());
	printer::inst().print(out_colours::K_MAGENTA, "Dataset calculation finished.");
	ready_id = seed.get_id();
}

void rx_dataset::calculate_thd(size_t idx, size_t thd_cnt)
{
	size_t work_size = randomx_dataset_item_count();
	size_t batch_size = work_size / thd_cnt;
	randomx_init_dataset(ds, ch, idx*batch_size, idx == thd_cnt-1 ? (batch_size+work_size%thd_cnt) : batch_size);
}
