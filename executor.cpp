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

void executor::close()
{
	exit(0);
}

void executor::run()
{
	uv_loop = uv_default_loop();
	in_console.init_console();

	uv_run(uv_loop, UV_RUN_DEFAULT);
}
