#pragma once

#include "miner.hpp"

class pp_cpu_miner : public miner
{
public:
	pp_cpu_miner(size_t thd_id) : miner(thd_id, miner_type::progpow_cpu_miner) {}
	
private:
	virtual void thread_main() override;
	void progpow_loop();
};

