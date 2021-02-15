#pragma once

#include <chrono>
#include <inttypes.h>

inline int64_t get_timestamp_s()
{
	using namespace std::chrono;
	return time_point_cast<seconds>(system_clock::now()).time_since_epoch().count();
};

inline int64_t get_timestamp_ms()
{
	using namespace std::chrono;
	return time_point_cast<milliseconds>(steady_clock::now()).time_since_epoch().count();
};
