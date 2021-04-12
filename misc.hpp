#pragma once

#include <chrono>
#include <array>
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

inline unsigned char hf_hex2bin(char c, bool& err)
{
	if(c >= '0' && c <= '9')
		return c - '0';
	else if(c >= 'a' && c <= 'f')
		return c - 'a' + 0xA;
	else if(c >= 'A' && c <= 'F')
		return c - 'A' + 0xA;
	
	err = true;
	return 0;
}

inline bool hex2bin(const char* in, unsigned int len, unsigned char* out)
{
	bool error = false;
	for(unsigned int i = 0; i < len; i += 2)
	{
		out[i / 2] = (hf_hex2bin(in[i], error) << 4) | hf_hex2bin(in[i + 1], error);
		if(error)
			return false;
	}
	return true;
}

inline char hf_bin2hex(unsigned char c)
{
	if(c <= 0x9)
		return '0' + c;
	else
		return 'a' - 0xA + c;
}

inline void bin2hex(const unsigned char* in, unsigned int len, char* out)
{
	for(unsigned int i = 0; i < len; i++)
	{
		out[i * 2] = hf_bin2hex((in[i] & 0xF0) >> 4);
		out[i * 2 + 1] = hf_bin2hex(in[i] & 0x0F);
	}
	out[len*2] = '\0';
}

template <typename T, size_t N>
class ring_buffer
{
public:
	ring_buffer() noexcept { }
	
	inline void insert(const T& val) noexcept
	{
		data[pos] = val;
		pos = (pos+1) % N;
	}

	inline T& operator[](size_t idx) noexcept
	{
		idx = (pos - idx - 1) % N;
		return data[idx];
	}

	inline const T& operator[](size_t idx) const noexcept
	{
		idx = (pos - idx - 1) % N;
		return data[idx];
	}
	
	inline void fill(const T& val) noexcept
	{
		data.fill(val);
	}
	
	inline size_t size() const noexcept
	{
		return N;
	}
	
private:
	size_t pos = 0;
	std::array<T, N> data;
};

struct human_ts
{
	human_ts(int64_t ts_sec)
	{
		secs = ts_sec % 60;
		ts_sec -= secs;
		mins = ts_sec % (60*60);
		ts_sec -= mins;
		hrs = ts_sec % (60*60*24);
		ts_sec -= hrs;
		days = ts_sec;
		mins /= 60;
		hrs  /= 60*60;
		days /= 24*60*60;
	}
	
	uint32_t days;
	uint32_t hrs;
	uint32_t mins;
	uint32_t secs;
};
