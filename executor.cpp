#include "console/console.hpp"
#include "executor.hpp"
#include "uv.hpp"
#include <stdio.h>

uv_loop_t* uv_loop = nullptr;

void executor::on_key_pressed(char key)
{
	if(key == 0x03)
		close();

	switch(key)
	{
		case 'h':
		case 'r':
			print_hashrate_report();
			print_uptime();
			print_share_totals();
			print_error_log();
			break;
		default:
			break;
	}
}

void executor::on_pool_new_job(uint32_t pool_id)
{
	printer::inst().print_dbg("pool %u has a new job!\n", pool_id);

	pool_job& job = pools[pool_id]->get_pool_job();
	if(job.type == pow_type::randomx && job.randomx_seed.get_id() != randomx_dataset.get_dataset_id())
	{
		printer::inst().print(out_colours::K_MAGENTA, "Calculating dataset... Mining will resume soon...");
		printer::inst().print_dbg("Calculating dataset %zx!\n", job.randomx_seed.get_id());
		push_idle_job();
		randomx_dataset.calculate_dataset(job.randomx_seed);
		return;
	}

	if(job.type == pow_type::randomx && !randomx_dataset.is_dataset_ready(job.randomx_seed.get_id()))
		return;

	uint32_t diff = (job.target > 0 ? (0xffffffff / job.target) : 0xffffffff);
	printer::inst().print(out_colours::K_BLUE, "Mining ", pow_type_to_str(job.type), " PoW, diff: ", diff);
	push_pool_job(job);
}

void executor::on_dataset_ready()
{
	printer::inst().print_dbg("Dataset ready %zx!\n", randomx_dataset.get_dataset_id());
	printer::inst().print(out_colours::K_MAGENTA, "Dataset calculation finished.");
	pool_job& job = pools[0]->get_pool_job();
	push_pool_job(job);
}

void executor::on_found_result(const result& res)
{
	pools[0]->do_send_result(res);
}

void executor::on_result_reply(uint32_t target, const char* error, uint64_t ping_ms)
{
	uint32_t diff = (target > 0 ? (0xffffffff / target) : 0xffffffff);
	if(error == nullptr)
	{
		pool_ctr.accepted_shares_cnt++;
		pool_ctr.total_diff += diff;
		print_accpted_share(diff, ping_ms);
	}
	else
	{
		pool_ctr.rejected_shares_cnt++;
		printer::inst().print(out_colours::K_RED, "Rejected share: ", error);
		log_error(error);
	}
}

void executor::close()
{
	in_console.close();
	quick_exit(0);
}

void executor::on_heartbeat()
{
	// Connection managment
	uint64_t connected_pools = 0;
	uint64_t connecting_pools = 0;
	for(auto& pl : pools)
	{
		pool::pool_state st = pl->get_pool_state();
		if(st == pool::pool_state::connected)
		{
			connected_pools++;
			pl->check_call_timeouts();
		}

		if(st == pool::pool_state::connecting)
			connecting_pools++;
	}

	if(connected_pools == 0 && connecting_pools == 0)
	{
		int64_t time_now = get_timestamp_ms();
		for(auto& pl : pools)
		{
			pool::pool_state st = pl->get_pool_state();
			if(st == pool::pool_state::idle && time_now - pl->get_last_connect_attempt() > 10000)
			{
				pl->do_connect();
				break;
			}
		}
	}

	record_miner_hashes();
}

void executor::run()
{
	uv_loop = uv_default_loop();
	in_console.init_console();
	push_idle_job();

	miners.emplace_back(std::make_unique<miner>(0));

	uv_timer_init(uv_loop, &heartbeat_timer);
	uv_timer_start(&heartbeat_timer, [](uv_timer_t*) { 
			executor::inst().on_heartbeat();
		}, 0, 1000);

	pools.emplace_back(std::make_unique<pool>(0));
	pools[0]->init_pool("88.80.191.147:3333", false, "", "user", "pass", "");

	uv_run(uv_loop, UV_RUN_DEFAULT);
}
