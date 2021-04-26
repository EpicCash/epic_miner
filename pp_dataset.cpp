#include "pp_dataset.hpp"
#include "executor.hpp"

#include <shared_mutex>
#include <mutex>

void pp_dataset::calculate_dataset(uint64_t epoch_num)
{
	loaded_id = epoch_num;
	ready_id = invalid_id;

	std::thread(&pp_dataset::dataset_thd_main, this, epoch_num).detach();
}

void pp_dataset::dataset_thd_main(uint64_t epoch_num)
{
	std::lock_guard<std::shared_timed_mutex> lk(mtx);
	printer::inst().print(out_colours::K_MAGENTA, "Calculating ProgPow dataset... Mining will resume soon...");
	printer::inst().print_dbg("Calculating dataset %zx!\n", epoch_num);

	if(dataset_ptr != nullptr)
		ethash_destroy_epoch_context_full(dataset_ptr);
	dataset_ptr = ethash_create_epoch_context_full(epoch_num);

	printer::inst().print_dbg("Dataset ready %zx!\n", epoch_num);
	printer::inst().print(out_colours::K_MAGENTA, "Dataset calculation finished.");
	ready_id = epoch_num;
}
