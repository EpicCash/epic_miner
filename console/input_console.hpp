#pragma once

#include <stdio.h>
#include "uv.hpp"

#ifdef _MSC_VER
#define fileno _fileno
#endif

class input_console
{
public:
	input_console() {}

	bool init_console()
	{
		stdin_fd = fileno(stdin);
		if(stdin_fd < 0)
			return false;

		uv_handle_type type = uv_guess_handle(stdin_fd);
		if(type != UV_TTY && type != UV_NAMED_PIPE)
			return false;

		console.data = this;
		uv_tty_init(uv_loop, &console, stdin_fd, 1);
		if(!uv_is_readable(reinterpret_cast<uv_stream_t*>(&console))) 
		{
			uv_close(reinterpret_cast<uv_handle_t*>(&console), nullptr);
			return false;
		}

		uv_tty_set_mode(&console, UV_TTY_MODE_RAW);
		uv_read_start(reinterpret_cast<uv_stream_t*>(&console), console_buffer, on_read);
		return true;
	}

	static void on_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);
	static void console_buffer(uv_handle_t *handle, size_t, uv_buf_t *buf);

private:
	uv_tty_t console;
	char buffer;
	int stdin_fd;
};
