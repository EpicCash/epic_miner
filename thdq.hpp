#pragma once 

#include <queue>
#include <list>
#include <thread>
#include <mutex>
#include <condition_variable>

enum class thdq_wait_rv
{
	is_finished = 0,
	timeout = 1,
	has_pop = 2
};

template <typename T>
class thdq
{
public:
	bool pop(T& item)
	{
		std::unique_lock<std::mutex> mlock(mutex_);
		while (queue_.empty() && !finish) { cond_.wait(mlock); }
		if(!queue_.empty())
		{
			item = std::move(queue_.front());
			queue_.pop();
			mlock.unlock();
			size_cond_.notify_all();
			return true;
		}
		return false;
	}

	bool wait_for_pop(std::unique_lock<std::mutex>& lck)
	{
		cond_.wait(lck, [&]{ return !queue_.empty() || finish; });
		bool has_pop = !queue_.empty();
		if(!has_pop) { lck.unlock(); }
		return has_pop;
	}

	template< class Rep, class Period >
	thdq_wait_rv wait_for_pop_timed(std::unique_lock<std::mutex>& lck, const std::chrono::duration<Rep, Period>& rel_time)
	{
		lck.lock();
		bool timeout = !cond_.wait_for(lck, rel_time, [&]{ return !queue_.empty() || finish; });
		
		if(timeout || queue_.empty())
		{
			lck.unlock();
			if(timeout)
				return thdq_wait_rv::timeout;
			else
				return thdq_wait_rv::is_finished;
		}
		return thdq_wait_rv::has_pop;
	}

	size_t wait_for_size(size_t q_size)
	{
		std::unique_lock<std::mutex> mlock(mutex_);
		while (queue_.size() > q_size) { size_cond_.wait(mlock); }
		return queue_.size();
	}

	T pop(std::unique_lock<std::mutex>& lck)
	{
		T item = std::move(queue_.front());
		queue_.pop();
		lck.unlock();
		size_cond_.notify_all();
		return item;
	}

	void push(const T& item)
	{
		std::unique_lock<std::mutex> mlock(mutex_);
		if(finish) return;
		queue_.push(item);
		mlock.unlock();
		cond_.notify_one();
	}

	void push(T&& item)
	{
		std::unique_lock<std::mutex> mlock(mutex_);
		if(finish) return;
		queue_.push(std::move(item));
		mlock.unlock();
		cond_.notify_one();
	}

	void set_finish_flag()
	{
		std::unique_lock<std::mutex> mlock(mutex_);
		finish = true;
		cond_.notify_all();
	}

	std::unique_lock<std::mutex> get_lock()
	{
		return std::unique_lock<std::mutex>(mutex_, std::defer_lock);
	}

private:
	std::queue<T> queue_;
	std::mutex mutex_;
	std::condition_variable cond_;
	std::condition_variable size_cond_;
	bool finish = false;
};
