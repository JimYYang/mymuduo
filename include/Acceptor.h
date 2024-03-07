#pragma once

#include "noncopyable.h"
#include "Socket.h"
#include "Channel.h"

#include <functional>

class EventLoop;
class InetAddress;

class Acceptor : noncopyable
{
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress&)>;
    Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport);
    ~Acceptor();

    void setNewConnnectinoCallback(const NewConnectionCallback &cb) {newConnectionCallback_ = std::move(cb);}

    bool listenning() const { return listenning_;}
    void listen();
private:
    void handleRead();

    // Acceptor用的就是用户定义的那个baseLoop 也就是mainLoop
    EventLoop *loop_;
    Socket acceptSocket_;
    Channel acceptChannel_;
    NewConnectionCallback newConnectionCallback_;
    bool listenning_;
};