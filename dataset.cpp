#include "dataset.hpp"
#include "executor.hpp"

#include <shared_mutex>
#include <mutex>

void dataset::calculate_dataset(const v32& seed)
{
	ready = false;
	this->seed = seed;
	this->id = seed.get_id();

	async.data = this;
	uv_async_init(uv_loop, &async, [](uv_async_t* handle) {
		dataset* ths = reinterpret_cast<dataset*>(handle->data);
		ths->dscalc.join();
		ths->ready = true;
		uv_close(reinterpret_cast<uv_handle_t*>(handle), nullptr);
		executor::inst().on_dataset_ready();
	});

	dscalc = std::thread(&dataset::dataset_thd_main, this);
}

void dataset::dataset_thd_main()
{
	randomx_init_cache(ch, seed.data, 32);
	
	std::vector<std::thread> thd;
	size_t thdcnt = std::thread::hardware_concurrency();
	thd.reserve(thdcnt-1);
	
	for(size_t i=1; i < thdcnt; i++)
		thd.emplace_back(&dataset::calculate_thd, this, i, thdcnt);
	
	calculate_thd(0, thdcnt);
	
	for(auto& t : thd)
		t.join();

	uv_async_send(&async);
}

void dataset::calculate_thd(size_t idx, size_t thd_cnt)
{
	size_t work_size = randomx_dataset_item_count();
	size_t batch_size = work_size / thd_cnt;
	randomx_init_dataset(ds, ch, idx*batch_size, idx == thd_cnt-1 ? (batch_size+work_size%thd_cnt) : batch_size);
}
