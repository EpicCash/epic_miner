#pragma once
#include <list>
#include <functional>
#include <unordered_map>

#include "rx_dataset.hpp"
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

	void on_found_result(const result& res);
	void on_result_reply(uint32_t target, const char* error, uint64_t ping_ms);

	inline miner_job* get_current_job()
	{
		return current_job.load(std::memory_order_relaxed);
	}

private:
	executor() {}
	void close();

	void on_heartbeat();

	void record_miner_hashes()
	{
		// Hashrate managment
		int64_t time_now = get_timestamp_ms();
		for(auto& mt : miners)
			mt->record_hashes(time_now);
	}

	void push_idle_job()
	{
		record_miner_hashes();
		pool_ctr.reset();
		miner_jobs.emplace_front();
		current_job = &miner_jobs.front();
	}

	void push_pool_job(pool_job& job)
	{
		record_miner_hashes();
		if(pool_ctr.session_timestamp == 0)
			pool_ctr.session_timestamp = get_timestamp_s();
		miner_jobs.emplace_front(job, &randomx_dataset, job.randomx_seed.get_id());
		current_job = &miner_jobs.front();
	}

	inline double get_hashrate(size_t thd_idx, uint64_t period_s)
	{
		if(thd_idx >= miners.size())
			return 0;

		const hashlog_t& hash_log = miners[thd_idx]->get_hash_log();
		if(hash_log[0].time == 0)
			return 0;

		uint64_t hash_count = 0;
		uint64_t time_count_ms = 0;
		for(size_t i = 1; i < hash_log.size(); i++)
		{
			if(hash_log[i].time == 0)
				break;

			if(hash_log[i-1].time > 0 && hash_log[i].time > 0)
			{
				int64_t time_delta = hash_log[i-1].time - hash_log[i].time;
				time_count_ms += uint64_t(time_delta);
				hash_count += uint64_t(hash_log[i-1].hashes - hash_log[i].hashes);

				if(time_count_ms > period_s * 1000)
					return (double(hash_count) / double(time_count_ms))*1000.0;
			}
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

	inline void print_accpted_share(uint32_t diff, uint64_t ping_ms)
	{
		double hps = 0;
		for(int i = 0; i < miners.size(); i++)
			hps += get_hashrate(i, 10);

		if(hps > 0)
		{
			printer::inst().print(out_colours::K_GREEN, "Share accepted (", pool_ctr.accepted_shares_cnt, 
						out_colours::K_RED, "/", pool_ctr.rejected_shares_cnt, ") ",
						out_colours::K_YELLOW, hps, " H/s",
						out_colours::K_BLUE, " diff: ", diff, " ping: ", ping_ms);
		}
		else
		{
			printer::inst().print(out_colours::K_GREEN, "Share accepted (", pool_ctr.accepted_shares_cnt, 
						out_colours::K_RED, "/", pool_ctr.rejected_shares_cnt, ")",
						out_colours::K_BLUE, " diff: ", diff, " ping: ", ping_ms);
		}
	}

	inline void print_hashrate_report()
	{
		std::string msg;
		msg.reserve(80*6 + miners.size()*80);
		
		msg += "HASHRATE REPORT\n";
		msg.append(77, '-');
		msg += "\n| THD | 10s cur. | 60s avg. | 10m avg. | 60m avg. |   Peak   | Session avg. |\n";
		
		int64_t session_time = 0;
		if(pool_ctr.session_timestamp != 0)
			session_time = get_timestamp_s() - pool_ctr.session_timestamp;
		
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

	inline void print_uptime()
	{
		if(pool_ctr.session_timestamp == 0)
		{
			printer::inst().print(out_colours::K_BLUE, "Session uptime: 00d 00h:00m:00s");
			return;
		}
		
		char buffer[128];
		int64_t session_time = get_timestamp_s() - pool_ctr.session_timestamp;
		human_ts ts(session_time);
		snprintf(buffer, sizeof(buffer),"%02dd %02dh:%02dm:%02ds", ts.days, ts.hrs, ts.mins, ts.secs);
		
		printer::inst().print_nt(out_colours::K_BLUE, "Session uptime: ", buffer);
	}
	
	inline void print_share_totals()
	{
		printer::inst().print_nt(out_colours::K_GREEN, "Accepted shares: ", pool_ctr.accepted_shares_cnt, " total diff. shares: ", pool_ctr.total_diff);
		printer::inst().print_nt(out_colours::K_RED, "Rejected shares: ", pool_ctr.rejected_shares_cnt);
	}

	inline void print_error_log()
	{
		if(error_log.size() == 0)
		{
			printer::inst().print_nt(out_colours::K_GREEN, "No errors to report.");
			return;
		}
		
		std::string msg;
		msg.reserve(80*3 + error_log.size() * 320);
		msg.append("ERROR LOG\n");
		msg.append(79, '-');
		msg.append("\n| ");
		msg.append(28, ' ');
		msg.append("Error");
		msg.append(28, ' ');
		msg.append("|  Last seen   |\n|");
		msg.append(62, ' ');
		msg.append("|    Count     |\n");
		msg.append(79, '-');
		msg.append("\n");
		
		char buf[128];
		int64_t cur_timestemp = get_timestamp_s();
		for(const auto& e : error_log)
		{
			bool long_in = false;
			human_ts ts(cur_timestemp - e.second.last_seen);
			const char* input = e.first.c_str();
			if(e.first.length() > 120)
			{
				snprintf(buf, sizeof(buf), "| %-60.60s |              |\n", input);
				input += 60;
				msg.append(buf);
				long_in = true;
			}
			
			snprintf(buf, sizeof(buf), "| %-60.60s | %03dh:%02dm:%02ds |\n", input, ts.days*24+ts.hrs, ts.mins, ts.secs);
			input += 60;
			msg.append(buf);
			snprintf(buf, sizeof(buf), "| %-60.60s |     %04lu     |\n", input, e.second.count);
			msg.append(buf);
			
			if(long_in)
			{
				if(e.first.length() > 180)
				{
					input += 60;
					snprintf(buf, sizeof(buf), "| %-60.60s |              |\n", input);
					msg.append(buf);
				}
				else
					msg.append("|                                                              |              |\n");
			}
			msg.append(79, '-');
			msg.append(1, '\n');
		}
		printer::inst().print_nt(msg.c_str());
	}

	struct session_counters_t
	{
		session_counters_t() { reset(); }

		int64_t session_timestamp;
		uint64_t accepted_shares_cnt;
		uint64_t rejected_shares_cnt;
		uint64_t total_diff;

		void reset()
		{
			session_timestamp = 0;
			accepted_shares_cnt = 0;
			rejected_shares_cnt = 0;
			total_diff = 0;
		}
	};

	session_counters_t pool_ctr;

	struct error_info
	{
		int64_t last_seen;
		size_t count=1;
	};

	std::unordered_map<std::string, error_info> error_log;

	std::vector<std::unique_ptr<pool>> pools;

	rx_dataset randomx_dataset;

	std::vector<std::unique_ptr<miner>> miners;
	// miner_job is immutable, once the atomic pointer is set
	// the threads can copy it without locks
	std::list<miner_job> miner_jobs;
	std::atomic<miner_job*> current_job;

	input_console in_console;

	uv_timer_t heartbeat_timer;
};
