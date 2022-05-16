#ifndef _HEAPTIMER_H
#define _HEAPTIMER_H

#include "./define.h"
#include <queue>
#include <unordered_map>
#include <arpa/inet.h> 
#include <functional> 
#include <chrono>

typedef std::function<void()> TimeoutCallBack;
typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::milliseconds MS;
typedef Clock::time_point TimeStamp;

struct TimerNode {
    int id;
    TimeStamp expires;
    TimeoutCallBack cb;
    bool operator<(const TimerNode& t) {
        return expires < t.expires;
    }
};

class HeapTimer {
public:
    HeapTimer() { m_heap.reserve(64); }
    ~HeapTimer() { clear(); }
    
    void adjust(int id, int newExpires);
    void add(int id, int timeOut, const TimeoutCallBack& cb);
    void doWork(int id);
    void clear();
    void tick();
    void pop();
    int GetNextTick();

private:
    std::vector<TimerNode> m_heap;
    std::unordered_map<int, size_t> m_ref;      // fd和堆节点索引的键值对

    void _Swap_Node(size_t i, size_t j);
    void _del(size_t i);
    void _sift_up(size_t i);
    bool _sift_down(size_t index, size_t n);
};

#endif /* HEAPTIMER_H */

