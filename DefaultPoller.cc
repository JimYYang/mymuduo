#include "Poller.h"
#include "EPollPoller.h"

#include <cstdlib>

// Poller是基类 这里返回派生类的实例 但是最好不要引用派生类的头文件 所以这个函数实现在公共文件里面
Poller* Poller::newDefaultPoller(EventLoop *loop)
{
    if (::getenv("MUDUO_USE_POLL"))
    {
        return nullptr; // 生成poll的实例
    }
    else
    {
        return new EPollPoller(loop); // 生成epoll的实例
    }
}