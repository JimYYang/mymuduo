#include "EventLoop.h"
#include "Logger.h"
#include "Poller.h"
#include "Channel.h"

#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <memory>

// 防止一个线程创建多个EventLoop thread_local
__thread EventLoop *t_loopInThisThread = nullptr;

// 定义默认的Poller IO复用接口的超时时间(ms)
const int kPollTimeMs = 10000;

// 创建wakeupfd 用来notify唤醒subReactor处理新来的channel
int createEventFd()
{
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0)
    {
        LOG_FATAL("eventfd error:%d \n", errno);
    }
    return evtfd;
}

EventLoop::EventLoop()
    : looping_(false), quit_(false), callingPendingFunctors_(false), threadId_(CurrentThread::tid()), poller_(Poller::newDefaultPoller(this)), wakeupFd_(createEventFd()), wakeupChannel_(new Channel(this, wakeupFd_))
{
    LOG_DEBUG("EventLoop created %p in thread %d \n", this, threadId_);
    if (t_loopInThisThread)
    {
        LOG_FATAL("Another EventLoop %p exists in this thread %d \n", t_loopInThisThread, threadId_);
    }
    else
    {
        t_loopInThisThread = this;
    }

    // 设置wakeupfd的事件类型以及发生事件后的回调操作
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    // 每一个eventloop都将监听wakeupchannel的EPOLLIN读事件了
    // mainReactor可以通过给wakeFd写东西来通知 subReactor起来
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop()
{
    wakeupChannel_->disableAll();
    // 把channle本身从poller中移除
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof one);
    if (n != sizeof one)
    {
        LOG_ERROR("EventLoop::handleRead() reads %lu bytes instead of 8", n);
    }
}

// 开启事件循环
void EventLoop::loop()
{
    looping_ = true;
    quit_ = false;

    LOG_INFO("EventLoop %p start looping \n", this);

    while (!quit_)
    {
        activeChannels_.clear();
        // 监听两类fd 一种是clientfd（正常和客户端通信用） 一种是wakeupfd（mainLoop和subLoop通信使用）
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        for (Channel *channel : activeChannels_)
        {
            // poller监听哪些channel发生事件了然后上报给EventLoop 通知channel处理相应的事件
            channel->handleEnvent(pollReturnTime_);
        }
        // 执行当前EventLoop事件循环需要处理的回调操作
        /**
         * IO 线程 mainLoop accept新连接 然后返回fd
         * 用channel打包fd
         * 已连接的channel分发给subLoop
         * (如果不调用setThreadNumber 只有一个mainLoop 既处理读写 又处理事件)
         * mainLoop拿到一个新的channel后 会唤醒某一个subLoop
         * mainLoop事先注册一个回调cb(需要subLoop来执行)
         * wakeuo subLoop后 执行下面的方法 即之前mainLoop注册的cb操作（可能是多个）
         */
        doPendingFunctors();
    }
    LOG_INFO("EventLoop %p stop looping. \n", this);
    looping_ = false;
}

// 退出事件循环 1. loop在自己的线程中调用quit(这时候肯定是已经从poll函数返回了)
/**
 *               mainLoop
 *  subLoop1    subLoop2    subLoop3 每个loop都有wakeupFd
 *  mainLoop和subLoop之间使用轮询算法通过wakeFd来协作
 */
void EventLoop::quit()
{
    quit_ = true;
    // 如果是在其他线程 调用quit 在subLoop(worker)中 调用mainLoop(IO)的quit 那么需要唤醒mainLoop
    if (!isInLoopThread())
    {
        wakeup();
    }
}

// 在当前loop中执行cb
void EventLoop::runInLoop(Functor cb)
{
    if (isInLoopThread()) // 在当前的loop线程中 执行cb
    {
        cb();
    }
    else // 在非当前loop中 执行cb 就需要唤醒loop所在线程 执行cb
    {
        queueInLoop(cb);
    }
}

// 把cb放入队列中 唤醒loop所在的线程 执行cb
void EventLoop::queueInLoop(Functor cb)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb); // == push_back(std::move(cb))
    }

    // 唤醒相应的 需要执行上面回调操作的loop线程
    // ||callPendingFunctors 含义：当前loop正在执行回调 但是loop又有了新的回调 所以需要唤醒一下
    if (!isInLoopThread() || callingPendingFunctors_)
    {
        wakeup(); // 唤醒loop所在线程
    }
}

// 用来唤醒loop所在的线程
// 向wakeFd写一个数据 wakeupChannel就有数据可读 发生读事件 当前loop线程就会被唤醒
void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof one);
    if (n != sizeof one)
    {
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of \n", n);
    }
}

// EventLoop的方法 => Poller的方法
void EventLoop::updateChannel(Channel *channel)
{
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel *channel)
{
    poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel *channel)
{
    return poller_->hasChannel(channel);
}

// 执行回调
void EventLoop::doPendingFunctors()
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        std::unique_lock<std::mutex> lock(mutex_);
        // 交换的原因是 可以让pendingFunctors_继续接收新的
        // 不需要等这里处理完  提高效率
        functors.swap(pendingFunctors_);
    }

    for (const Functor functor : functors)
    {
        functor(); // 执行当前loop需要执行的回调操作
    }

    callingPendingFunctors_ = false;
}