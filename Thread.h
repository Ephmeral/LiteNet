#pragma once 

#include <functional>
#include <memory>
#include <unistd.h>
#include <atomic>
#include "nocopyable.h"

class Thread {
public:
    using ThreadFunc = std::function<void()>;

    Thread(ThreadFunc, const std::string &name = string());
    ~Thread();

    void start();
    void join();

    bool stated() const { return started_; }
    pid_t tid() const { return tid_; }
    const std::string& name() const { return name_; }

    static int numCreated() { return numCreated_; }

private:
    void setDefaultName();

    bool started_;
    bool joined_;
    std::shared_ptr<std::thread> thread_;

    pid_t tid;
    ThreadFunc func_;
    std::string name_;
    static std::atomic_int32 numCreated_;

};