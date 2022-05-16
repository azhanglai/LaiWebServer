#include "../include/epoller.h"

// epoll_create: 创建epoll红黑树
// 并且初始化epoll_event 数组
Epoller::Epoller(int maxEvent):m_epollFd(epoll_create(1)), m_events(maxEvent){
    assert(m_epollFd >= 0 && m_events.size() > 0);
}

Epoller::~Epoller() {
    close(m_epollFd);
}

// 将fd的events事件 注册到 epoll
bool Epoller::AddFd(int fd, uint32_t events) {
    if(fd < 0) return false;
    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;
    return 0 == epoll_ctl(m_epollFd, EPOLL_CTL_ADD, fd, &ev);
}

// 更改epoll中 fd的events事件
bool Epoller::ModFd(int fd, uint32_t events) {
    if(fd < 0) return false;
    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;
    return 0 == epoll_ctl(m_epollFd, EPOLL_CTL_MOD, fd, &ev);
}

// 将注册到epoll中的fd删除
bool Epoller::DelFd(int fd) {
    if(fd < 0) return false;
    epoll_event ev = {0};
    return 0 == epoll_ctl(m_epollFd, EPOLL_CTL_DEL, fd, &ev);
}

// epoll_wait: 等待timeout时间，注册到epoll里面的事件如果有就绪的会添加到m_events中 
int Epoller::Wait(int timeout) {
    return epoll_wait(m_epollFd, &m_events[0], static_cast<int>(m_events.size()), timeout);
}

// 得到m_events[i]中的fd 
int Epoller::GetEventFd(size_t i) const {
    assert(i < m_events.size() && i >= 0);
    return m_events[i].data.fd;
}

// 得到m_events[i]中的事件
uint32_t Epoller::GetEvents(size_t i) const {
    assert(i < m_events.size() && i >= 0);
    return m_events[i].events;
}

