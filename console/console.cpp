#include "console.hpp"

#include <cstdlib>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>

int get_key()
{
	DWORD mode, rd;
	HANDLE h;
	
	if((h = GetStdHandle(STD_INPUT_HANDLE)) == NULL)
		return -1;
	
	GetConsoleMode(h, &mode);
	SetConsoleMode(h, mode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT));
	
	int c = 0;
	ReadConsole(h, &c, 1, &rd, NULL);
	SetConsoleMode(h, mode);
	
	return c;
}

void set_colour(out_colours cl)
{
	WORD attr = 0;
	
	switch(cl)
	{
		case K_RED:
			attr = FOREGROUND_RED | FOREGROUND_INTENSITY;
			break;
		case K_GREEN:
			attr = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
			break;
		case K_BLUE:
			attr = FOREGROUND_BLUE | FOREGROUND_INTENSITY;
			break;
		case K_YELLOW:
			attr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
			break;
		case K_CYAN:
			attr = FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
			break;
		case K_MAGENTA:
			attr = FOREGROUND_BLUE | FOREGROUND_RED | FOREGROUND_INTENSITY;
			break;
		case K_WHITE:
			attr = FOREGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
			break;
		default:
			break;
	}
	
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), attr);
}

void reset_colour()
{
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}

void win_exit(int code)
{
	size_t envSize = 0;
	getenv_s(&envSize, nullptr, 0, "EPIC_NOWAIT");
	if(envSize == 0)
	{
		printer::inst()->print_str("Press any key to exit.");
		get_key();
	}
	std::exit(code);
}

#else

#include <stdio.h>
#include <termios.h>
#include <unistd.h>

int get_key()
{
	struct termios oldattr, newattr;
	int ch;
	tcgetattr(STDIN_FILENO, &oldattr);
	newattr = oldattr;
	newattr.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newattr);
	ch = getchar();
	tcsetattr(STDIN_FILENO, TCSANOW, &oldattr);
	return ch;
}

void set_colour(out_colours cl)
{
	switch(cl)
	{
		case K_RED:
			fputs("\x1B[1;31m", stdout);
			break;
		case K_GREEN:
			fputs("\x1B[1;32m", stdout);
			break;
		case K_BLUE:
			fputs("\x1B[1;34m", stdout);
			break;
		case K_YELLOW:
			fputs("\x1B[1;33m", stdout);
			break;
		case K_CYAN:
			fputs("\x1B[1;36m", stdout);
			break;
		case K_MAGENTA:
			fputs("\x1B[1;35m", stdout);
			break;
		case K_WHITE:
			fputs("\x1B[1;37m", stdout);
			break;
		default:
			break;
	}
}

void reset_colour()
{
	fputs("\x1B[0m", stdout);
	fflush(stdout);
}

void win_exit(int code)
{
	std::exit(code);
}
#endif // _WIN32

printer::printer()
{
	logfile = nullptr;
	// Enable full buffering and manually flush the buffer
	setvbuf(stdout, NULL, _IOFBF, BUFSIZ);
}

bool printer::open_logfile(const char* file)
{
	logfile = fopen(file, "wb");
	return logfile != nullptr;
}

void printer::print_msg(const char* fmt, ...)
{
	char buf[1024];
	size_t bpos;
	tm stime;

	time_t now = time(nullptr);
	comp_localtime(&now, &stime);
	strftime(buf, sizeof(buf), "[%F %T]: ", &stime);
	bpos = strlen(buf);

	va_list args;
	va_start(args, fmt);
	vsnprintf(buf + bpos, sizeof(buf) - bpos, fmt, args);
	va_end(args);
	bpos = strlen(buf);

	if(bpos + 2 >= sizeof(buf))
		return;

	buf[bpos] = '\n';
	buf[bpos + 1] = '\0';

	print_str(buf);
}

#ifdef IS_DEBUG
void printer::print_dbg(const char* fmt, ...)
{
	char buf[1024];
	size_t bpos;
	tm stime;
	
	time_t now = time(nullptr);
	comp_localtime(&now, &stime);
	strftime(buf, sizeof(buf), "[%F %T]: ", &stime);
	bpos = strlen(buf);
	
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf + bpos, sizeof(buf) - bpos, fmt, args);
	va_end(args);
	bpos = strlen(buf);
	
	if(bpos + 2 >= sizeof(buf))
		return;
	
	buf[bpos] = '\n';
	buf[bpos + 1] = '\0';
	
	print_str(buf);
}
#endif

void printer::print_str(const char* str)
{
	std::unique_lock<std::mutex> lck(print_mutex);
	fputs(str, stdout);
	fflush(stdout);

	if(logfile != nullptr)
	{
		fputs(str, logfile);
		fflush(logfile);
	}
}

void printer::print_str(const out_colours& colour, const char* str)
{
	std::unique_lock<std::mutex> lck(print_mutex);
	set_colour(colour);
	fputs(str, stdout);
	fflush(stdout);

	if(logfile != nullptr)
	{
		fputs(str, logfile);
		fflush(logfile);
	}
	reset_colour();
}
