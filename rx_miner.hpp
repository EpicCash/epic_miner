#pragma once

#include "miner.hpp"

class rx_cpu_miner : public miner
{
public:
	rx_cpu_miner(size_t thd_id) : miner(thd_id) {}

private:
	virtual void thread_main() override;
	void randomx_loop();
};
