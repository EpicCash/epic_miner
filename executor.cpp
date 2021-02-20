#include "console/console.hpp"
#include "executor.hpp"
#include <uv.h>

uv_loop_t* uv_loop = nullptr;

void executor::run()
{
	uv_loop = uv_default_loop();
}
