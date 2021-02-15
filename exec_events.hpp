#pragma once

#include <inttypes.h>

enum class exec_msg_type : uint32_t
{
	exec_event_on_net_msg,
	exec_event_on_result,
	exec_event_on_dataset_finished,
	exec_event_on_disconnect,
	exec_try_connect,
	exec_event_print_hashrate
};

struct exec_message
{
	exec_message(exec_msg_type type) : type(type) {}
	virtual ~exec_message() {}

	exec_msg_type type;
};

