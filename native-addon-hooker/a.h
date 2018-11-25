#include <node_api.h>
#include <deque>
#include <mutex>
#include <assert.h>
#include <iostream>

using namespace std;

template <typename Data>
class PCQueue
{
public:
  void write(Data data)
  {
    std::unique_lock<std::mutex> locker(mu);
    buffer_.push_back(data);
    locker.unlock();
    cond.notify_all();
    return;
  }

  void destroy()
  {
    destroyed = true;
	cond.notify_all();
  }
  
  Data* read()
  {
    std::unique_lock<std::mutex> locker(mu);
    cond.wait(locker, [this]() { return buffer_.size() > 0 || destroyed; });
	if(!destroyed)
	{
		Data back = buffer_.front();
		buffer_.pop_front();
		locker.unlock();
		cond.notify_all();
		return &back;
	}
	else
	{
		locker.unlock();
		cond.notify_all();
		return NULL;
	}
  }
  
  void readAll(std::deque<Data> &target)
  {
    std::unique_lock<std::mutex> locker(mu);
    std::copy(buffer_.begin(), buffer_.end(), std::back_inserter(target));
    buffer_.clear();
    locker.unlock();
  }

  PCQueue() {}

private:
  bool destroyed;
  std::mutex mu;
  std::condition_variable cond;
  std::deque<Data> buffer_;
};

class Message
{
public:
  string data;
  Message(string data) : data(data) {};
};

PCQueue<Message> clicksQueue;