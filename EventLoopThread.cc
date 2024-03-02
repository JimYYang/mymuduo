#include "EventLoopThread.h"
#include "EventLoop.h"

EventLoopThread::EventLoopThread(const ThreadInitCallback &cb = ThreadInitCallback(),
                                 const std::string &name = std::string())
                                 : loop_(nullptr)
                                 , exiting_(false)
                                 , thread_(std::bind(&EventLoopThread::threadFunc, this), name)
                                 , mutex_()
                                 , cond_()
                                 , callback_(cb)
{
}

EventLoopThread::~EventLoopThread()
{
    exiting_ = true;
    if (loop_ != nullptr)
    {
        loop_->quit();
        thread_.join();
    }
}

EventLoop *EventLoopThread::startLoop()
{
    thread_.start(); // 启动新线程

    EventLoop *loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while (loop_ = nullptr)
        {
            cond_.wait(lock);
        }
        loop = loop_;
    }
    return loop;
}

// 这个函数是在单独的新线程中运行的
void EventLoopThread::threadFunc()
{
    // 创建一个独立的EventLoop 和上面的线程是一一对应的
    // one loop per thread 
    EventLoop loop;

    if (callback_)
    {
        callback_(&loop);
    }

    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();
    }

    loop.loop(); // EventLoop loop => Poller.poll 进入阻塞 监听远端用户的连接或者已连接用户的读写事件
    // 进行到下面说明服务要关闭了 不进行事件循环了
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr;
}
