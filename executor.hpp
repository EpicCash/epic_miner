#pragma once
#include <list>
#include <functional>
#include <unordered_map>

#include "dataset.hpp"
#include "pool.hpp"
#include "miner.hpp"
#include "console/input_console.hpp"
#include "misc.hpp"

class executor
{
public:
	static executor& inst()
	{
		static executor inst;
		return inst;
	};

	void run();

	inline void log_error(std::string&& error_msg)
	{
		auto res = error_log.emplace(std::piecewise_construct,
									 std::forward_as_tuple(std::move(error_msg)),
									 std::forward_as_tuple());
		if(!res.second)
			res.first->second.count++;
		
		res.first->second.last_seen = get_timestamp_s();
	}

	inline void log_error(const char* error_msg)
	{
		auto res = error_log.emplace(std::piecewise_construct,
									 std::forward_as_tuple(error_msg),
									 std::forward_as_tuple());
		if(!res.second)
			res.first->second.count++;
		
		res.first->second.last_seen = get_timestamp_s();
	}

	void on_key_pressed(char key);

	void on_pool_new_job(uint32_t pool_id);

	void on_dataset_ready();
	void on_found_result(const result& res);

	inline miner_job* get_current_job()
	{
		return current_job.load(std::memory_order_relaxed);
	}

private:
	executor() {}
	void close();

	void on_heartbeat();

	void push_idle_job()
	{
		session_timestamp = 0;
		miner_jobs.emplace_front();
		current_job = &miner_jobs.front();
	}

	void push_pool_job(pool_job& job)
	{
		if(session_timestamp == 0)
			session_timestamp = get_timestamp_s();
		miner_jobs.emplace_front(job, &randomx_dataset);
		current_job = &miner_jobs.front();
	}

	inline double get_hashrate(size_t thd_idx, uint64_t period_s)
	{
		if(thd_idx >= miners.size())
			return 0;

		const hashlog_t& hash_log = miners[thd_idx]->get_hash_log();
		for(size_t i = 0; i < hash_log.size(); i++)
		{
			if(hash_log[i].time == 0)
				break;
			
			size_t delta = hash_log[0].time - hash_log[i].time;
			if(delta > period_s * 1000)
				return (double(hash_log[0].hashes - hash_log[i].hashes) / double(delta))*1000.0;
		}
		return 0;
	}

	inline double get_hashrate_peak(size_t thd_idx)
	{
		if(thd_idx >= miners.size())
			return 0;

		size_t p=0;
		double peak_hps = 0.0;
		const hashlog_t& hash_log = miners[thd_idx]->get_hash_log();
		for(size_t i = 0; i < hash_log.size(); i++)
		{
			if(hash_log[i].time == 0)
				break;

			size_t delta = hash_log[p].time - hash_log[i].time;
			if(delta > 10000)
			{
				double hps = ((double(hash_log[p].hashes - hash_log[i].hashes)) / double(delta))*1000.0;
				if(hps > peak_hps)
					peak_hps = hps;
				p = i;
			}
		}

		return peak_hps;
	}

	inline void print_hashrate_report()
	{
		std::string msg;
		msg.reserve(80*6 + miners.size()*80);
		
		msg += "HASHRATE REPORT\n";
		msg.append(77, '-');
		msg += "\n| THD | 10s cur. | 60s avg. | 10m avg. | 60m avg. |   Peak   | Session avg. |\n";
		
		int64_t session_time = 0;
		if(session_timestamp != 0)
			session_time = get_timestamp_s() - session_timestamp;
		
		if(session_time > 24*60*60)
			session_time = 24*60*60;
		
		char buffer[128];
		size_t len;
		double sum_hr_10s = 0.0, sum_hr_60s = 0.0, sum_hr_10m = 0.0; 
		double sum_hr_60m = 0.0, tot_hr_peak = 0.0, sum_hr_sess = 0.0;
		for(size_t i = 0; i < miners.size(); i++)
		{
			double hr_10s, hr_60s, hr_10m, hr_60m, hr_peak, hr_sess;
			hr_10s  = get_hashrate(i, 10);
			hr_60s  = get_hashrate(i, 60);
			hr_10m  = get_hashrate(i, 10 * 60);
			hr_60m  = get_hashrate(i, 60 * 60);
			hr_peak = get_hashrate_peak(i);
			hr_sess = session_time > 45 ? get_hashrate(i, session_time-30) : 0.0;
			
			len = snprintf(buffer, sizeof(buffer), "|%4ld |%9.1f |%9.1f |%9.1f |%9.1f |%9.1f |%13.1f |\n", 
						   i, hr_10s, hr_60s, hr_10m, hr_60m, hr_peak, hr_sess);
			msg.append(buffer, len);
			
			sum_hr_10s += hr_10s;
			sum_hr_60s += hr_60s;
			sum_hr_10m += hr_10m;
			sum_hr_60m += hr_60m;
			tot_hr_peak = std::max(tot_hr_peak, hr_peak);
			sum_hr_sess = hr_sess;
		}
		
		msg.append(77, '-');
		msg.append(1, '\n');
		
		len = snprintf(buffer, sizeof(buffer), "| SUM |%9.1f |%9.1f |%9.1f |%9.1f |%9.1f |%13.1f |\n", 
					   sum_hr_10s, sum_hr_60s, sum_hr_10m, sum_hr_60m, tot_hr_peak, sum_hr_sess);
		msg.append(buffer, len);
		msg.append(77, '-');
		
		printer::inst().print_nt(msg.data());
	}

	struct error_info
	{
		int64_t last_seen;
		size_t count=1;
	};

	int64_t session_timestamp = 0;

	std::unordered_map<std::string, error_info> error_log;

	std::vector<std::unique_ptr<pool>> pools;

	dataset randomx_dataset;

	std::vector<std::unique_ptr<miner>> miners;
	// miner_job is immutable, once the atomic pointer is set
	// the threads can copy it without locks
	std::list<miner_job> miner_jobs;
	std::atomic<miner_job*> current_job;

	input_console in_console;

	uv_timer_t heartbeat_timer;
};
