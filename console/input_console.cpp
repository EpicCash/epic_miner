#include "input_console.hpp"
#include "executor.hpp"

void input_console::on_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
{
	if(nread < 0) 
		uv_close(reinterpret_cast<uv_handle_t*>(stream), nullptr);
	else if(nread == 1) 
		executor::inst().on_key_pressed(*buf->base);
}

void input_console::console_buffer(uv_handle_t *handle, size_t, uv_buf_t *buf)
{
	input_console* ths = reinterpret_cast<input_console*>(handle->data);
	buf->len  = 1;
	buf->base = &ths->buffer;
}
