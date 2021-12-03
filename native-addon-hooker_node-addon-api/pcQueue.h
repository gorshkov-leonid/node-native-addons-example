// https://github.com/freezer333/nodecpp-demo/blob/9130c01631ba20048c3fffc3632eadcb94971724/streaming/dist/streaming-worker.h
#include <iostream>
#include <string>
#include <algorithm>
#include <iterator>
#include <thread>
#include <deque>
#include <mutex>
#include <chrono>
#include <condition_variable>

using namespace std;

template<typename Data>
class PCQueue {
public:
    void write(Data data) {
        std::unique_lock<std::mutex> locker(mu);
        buffer_.push_back(data);
        locker.unlock();
        cond.notify_all();
    }

    Data read() {
        std::unique_lock<std::mutex> locker(mu);
        cond.wait(locker, [this]() { return buffer_.size() > 0 || doBreak; });
        if (doBreak) {
            return NULL;
        }
        Data front = buffer_.back();
        buffer_.pop_back();
        locker.unlock();
        cond.notify_all();
        return front;

    }

    void readAll(std::deque<Data> &target) {
        std::unique_lock<std::mutex> locker(mu);
        std::copy(buffer_.begin(), buffer_.end(), std::back_inserter(target));
        buffer_.clear();
        locker.unlock();
    }

    void destroy() {
        std::unique_lock<std::mutex> locker(mu);
        doBreak = true;
        buffer_.clear();
        cond.notify_all();
        locker.unlock();
    }

    PCQueue() {}

private:
    std::deque<Data> buffer_;
    std::mutex mu;
    std::condition_variable cond;
    bool doBreak;
};