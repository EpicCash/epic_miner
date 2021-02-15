#include "console/console.hpp"
#include "executor.hpp"

void executor::run()
{
	std::unique_lock<std::mutex> q_lck = event_q.get_lock();
	thdq_wait_rv rv;
	while((rv = event_q.wait_for_pop_timed(q_lck, std::chrono::milliseconds(2500))) != thdq_wait_rv::is_finished)
	{
		printer::inst().print_dbg("exec_event");
		if(rv == thdq_wait_rv::has_pop)
			on_executor_event(event_q.pop(q_lck));
	}
}

void on_executor_event(const std::unique_ptr<exec_message>& event)
{
	
}
