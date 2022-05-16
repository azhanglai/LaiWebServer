#ifndef _EPOLLER_H
#define _EPOLLER_H

#include "./define.h"
#include <vector>
#include <errno.h>

class Epoller {
public:
    explicit Epoller(int maxEvent = 1024);

    ~Epoller();

    bool AddFd(int fd, uint32_t events);

    bool ModFd(int fd, uint32_t events);

    bool DelFd(int fd);

    int Wait(int timeout = -1);

    int GetEventFd(size_t i) const;

    uint32_t GetEvents(size_t i) const;

private:
    int m_epollFd;
    std::vector<struct epoll_event> m_events;    
};

#endif /* _EPOLLER_H */

