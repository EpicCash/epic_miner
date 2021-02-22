#pragma once
#include <mutex>
#include <time.h>

enum out_colours : uint8_t
{
	K_RED = 7,
	K_GREEN = 6,
	K_BLUE = 5,
	K_YELLOW = 4,
	K_CYAN = 3,
	K_MAGENTA = 2,
	K_WHITE = 1,
	K_NONE = 0
};

// Alphanum keys only.
int get_key();

void set_colour(out_colours cl);
void reset_colour();

inline void comp_localtime(const time_t* ctime, tm* stime)
{
#ifdef _WIN32
	localtime_s(stime, ctime);
#else
	localtime_r(ctime, stime);
#endif // __WIN32
}

class printer
{
public:
	static inline printer& inst()
	{
		static printer inst;

		return inst;
	};

	void print_msg(const char* fmt, ...);
	void print_str(const char* str);
	void print_str(const out_colours& colour, const char* str);
	bool open_logfile(const char* file);

#ifdef IS_DEBUG
	void print_dbg(const char* fmt, ...);
#else
	inline void print_dbg(const char* fmt, ...) {}
#endif

	template<typename... Args>
	inline void print(Args... args)
	{
		char buf[64];
		tm stime;
		time_t now = time(nullptr);
		comp_localtime(&now, &stime);
		strftime(buf, sizeof(buf), "[%F %T]: ", &stime);

		std::unique_lock<std::mutex> lck(print_mutex);
		parse_print_arg(buf);
		parse_print(args...);
		reset_colour();
	}

	template<typename... Args>
	inline void print_nt(Args... args)
	{
		std::unique_lock<std::mutex> lck(print_mutex);
		parse_print(args...);
		reset_colour();
	}

private:
	printer();

	inline void parse_print()
	{
		parse_print_arg("\n");
		fflush(stdout);
		if(logfile != nullptr)
			fflush(logfile);
	}

	template<typename Arg1, typename... Args>
	inline void parse_print(Arg1& arg1, Args&... args)
	{
		parse_print_arg(arg1);
		parse_print(args...);
	}

	inline void parse_print_arg(const out_colours& colour)
	{
		set_colour(colour);
	}

	inline void parse_print_arg(const char* str)
	{
		fputs(str, stdout);
		if(logfile != nullptr)
			fputs(str, logfile);
	}

	inline void parse_print_arg(double v)
	{
		char buf[32];
		snprintf(buf, sizeof(buf), "%.1f", v);
		parse_print_arg(buf);
	}

	inline void parse_print_arg(const std::string& str)
	{
		parse_print_arg(str.data());
	}

	inline void parse_print_arg(size_t arg)
	{
		char buf[32];
		snprintf(buf, sizeof(buf), "%lu", arg);
		parse_print_arg(buf);
	}

	inline void parse_print_arg(int64_t arg)
	{
		char buf[32];
		snprintf(buf, sizeof(buf), "%ld", arg);
		parse_print_arg(buf);
	}

	inline void parse_print_arg(uint32_t arg)
	{
		char buf[32];
		snprintf(buf, sizeof(buf), "%u", arg);
		parse_print_arg(buf);
	}

	std::mutex print_mutex;
	FILE* logfile;
};
