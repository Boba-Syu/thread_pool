#ifndef CPP_TEST_THREADTEST_H
#define CPP_TEST_THREADTEST_H

#include <memory>
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <functional>
#include <future>
#include <algorithm>

class Queue {
public:
    void push(std::function<void(int)> *&value) {
        std::unique_lock<std::mutex> lock(mtx_);
        queue_.push(value);
    }

    bool pop(std::function<void(int)> *&value) {
        if (queue_.empty()) {
            return false;
        }
        value = queue_.front();
        queue_.pop();
        return true;
    }

    bool pop() {
        if (queue_.empty()) {
            return false;
        }
        queue_.pop();
        return true;
    }

    bool empty() const {
        return queue_.empty();
    }

    size_t size() const {
        return queue_.size();
    }


    void clear() {
        std::function<void(int)> *f;
        while (pop(f)) {
            delete f;
        }
    }

private:
    std::mutex mtx_;
    std::queue<std::function<void(int)> *> queue_;
};

class ThreadPool {
public:
    ThreadPool() : ThreadPool(8) {}

    ThreadPool(int);

    ~ThreadPool() {
        while (!fQueue_.empty());
        stop();
    }

    template<class F>
    auto push(F &&f) -> std::future<decltype(f(0))> {
        auto pck = std::make_shared<std::packaged_task<decltype(f(0))(int)>>(std::forward<F>(f));
        auto fun = new std::function<void(int)>([pck](int threadId) {
            (*pck)(threadId);
        });
        fQueue_.push(fun);
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.notify_one();
        return pck->get_future();
    }

    template<class F, class... Rest>
    auto push(F &&f, Rest &&... rest) -> std::future<decltype(f(0, rest...))> {
        auto pck = std::make_shared<std::packaged_task<decltype(f(0, rest...))(int)>>(
                std::bind(std::forward<F>(f), std::placeholders::_1, std::forward<Rest>(rest)...));
        auto fun = new std::function<void(int)>([pck](int threadId) {
            (*pck)(threadId);
        });
        fQueue_.push(fun);
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.notify_one();
        return pck->get_future();
    }

    void stop();

private:
    void setThread(int);

    // 线程池队列
    std::vector<std::unique_ptr<std::thread>> threads_;
    // 任务队列
    Queue fQueue_;
    std::mutex mtx_;
    std::condition_variable cv_;
    // 没有收到处理任务的等待线程
    std::atomic<int> nWaiting_{};
    // ture表示对应线程终止
    std::vector<std::shared_ptr<std::atomic<bool>>> flags_{};
    // true表示该线池停止
    bool stop_;
};


ThreadPool::ThreadPool(int size) : stop_(false) {
    threads_.resize(size);
    flags_.resize(size);
    nWaiting_ = 0;
    for (int i = 0; i < size; i++) {
        flags_[i] = std::make_shared<std::atomic<bool>>(false);
        setThread(i);
    }
}

void ThreadPool::setThread(int i) {
    auto fun = [this, i] {
        std::function<void(int)> *fun;
        bool isPop = fQueue_.pop(fun);
        while (true) {
            while (isPop) {
                std::unique_ptr<std::function<void(int)>> func_(fun);
                (*func_)(i);
                if (*flags_[i]) return;
                else isPop = fQueue_.pop(fun);
            }
            std::unique_lock<std::mutex> lock(mtx_);
            ++nWaiting_;
            cv_.wait(lock, [this, &isPop, &fun, i] {
                isPop = fQueue_.pop(fun);
                return isPop || stop_ || *flags_[i];
            });
            --nWaiting_;
            if (!isPop) return;
        }
    };
    threads_[i] = std::make_unique<std::thread>(fun);
}

void ThreadPool::stop() {
    stop_ = true;
    for (auto &flag: flags_)
        *flag = true;
    {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.notify_all();
    }
    for (int i = 0; i < threads_.size(); i++)
        if (threads_[i]->joinable()) threads_[i]->join();
    threads_.clear();
    flags_.clear();
    fQueue_.clear();
}


#endif //CPP_TEST_THREADTEST_H
