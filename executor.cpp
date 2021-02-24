#include "console/console.hpp"
#include "executor.hpp"
#include "uv.hpp"
#include <stdio.h>

uv_loop_t* uv_loop = nullptr;

void executor::on_key_pressed(char key)
{
	if(key == 0x03)
		close();

	printf("key: %.2x\n", (uint32_t)key);
}

void executor::on_pool_new_job(uint32_t pool_id)
{
	printf("pool %u has a new job!\n", pool_id);
	miner_job bcast(pools[0]->get_pool_job());
	std::optional<miner_job> t(bcast);
	printf("%u\n", t.has_value());
	//miner_job t = bcast;
}

void executor::close()
{
	in_console.close();
	exit(0);
}

void executor::on_heartbeat()
{
	printf("on_heartbeat\n");
}

void executor::run()
{
	uv_loop = uv_default_loop();
	in_console.init_console();
	push_idle_job();

	uv_timer_init(uv_loop, &heartbeat_timer);
	uv_timer_start(&heartbeat_timer, [](uv_timer_t*) { 
			executor::inst().on_heartbeat();
		}, 0, 1000);

	pools.emplace_back(std::make_unique<pool>(0));
	pools[0]->init_pool("localhost:3333", false, "user", "pass");
	pools[0]->do_connect();

	uv_run(uv_loop, UV_RUN_DEFAULT);
}
