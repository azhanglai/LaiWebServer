#include "../include/heaptimer.h"

// 两个节点交换位置
void HeapTimer::_Swap_Node(size_t i, size_t j) {
    assert(i >= 0 && i < m_heap.size());
    assert(j >= 0 && j < m_heap.size());
    std::swap(m_heap[i], m_heap[j]);
    m_ref[m_heap[i].id] = i;
    m_ref[m_heap[j].id] = j;
} 

// 较小节点向上调整
void HeapTimer::_sift_up(size_t i) {
    assert(i >= 0 && i < m_heap.size());
    size_t j = (i - 1) / 2;
    // 小顶堆，较小值节点往上调整
    while(j >= 0) {
        if(m_heap[j] < m_heap[i]) { break; }
        _Swap_Node(i, j);
        i = j;
        j = (i - 1) / 2;
    }
}

// 较大节点向下调整, 指定节点先下调整
bool HeapTimer::_sift_down(size_t index, size_t n) {
    assert(index >= 0 && index < m_heap.size());
    assert(n >= 0 && n <= m_heap.size());
    size_t i = index;
    size_t j = i * 2 + 1;
    while(j < n) {
        if(j + 1 < n && m_heap[j + 1] < m_heap[j]) j++;
        if(m_heap[i] < m_heap[j]) break;
        _Swap_Node(i, j);
        i = j;
        j = i * 2 + 1;
    }
    return i > index;
}

// 往小顶时间堆里 加入客户定时器
void HeapTimer::add(int id, int timeout, const TimeoutCallBack& cb) {
    assert(id >= 0);
    size_t i;
    if(m_ref.count(id) == 0) {
        // 新节点：堆尾插入，往上调整堆
        i = m_heap.size();
        m_ref[id] = i;
        m_heap.push_back({id, Clock::now() + MS(timeout), cb});
        _sift_up(i);
    } 
    else {
        // 已有节点：往下调整堆, 往下调整失败再试试往上调整
        i = m_ref[id];
        // 超时时间更改
        m_heap[i].expires = Clock::now() + MS(timeout);
        m_heap[i].cb = cb;
        if(!_sift_down(i, m_heap.size())) {
            _sift_up(i);
        }
    }
}

void HeapTimer::doWork(int id) {
    // 删除指定id结点，并触发回调函数
    if(m_heap.empty() || m_ref.count(id) == 0) {
        return;
    }
    size_t i = m_ref[id];
    TimerNode node = m_heap[i];
    node.cb();
    _del(i);
}

// 删除指定位置的结点 
void HeapTimer::_del(size_t index) {
    assert(!m_heap.empty() && index >= 0 && index < m_heap.size());
    // 将要删除的结点换到队尾，然后调整堆 
    size_t i = index;
    size_t n = m_heap.size() - 1;
    assert(i <= n);
    if(i < n) {
        _Swap_Node(i, n);       // 小根节点和尾节点呼唤
        if(!_sift_down(i, n)) {
            _sift_up(i);
        }
    }
    // 队尾元素删除 
    m_ref.erase(m_heap.back().id);
    m_heap.pop_back();
}

// 根据id和超时时间调整节点的位置
void HeapTimer::adjust(int id, int timeout) {
    assert(!m_heap.empty() && m_ref.count(id) > 0);
    m_heap[m_ref[id]].expires = Clock::now() + MS(timeout);
    _sift_down(m_ref[id], m_heap.size());
}

// 清除超时结点
void HeapTimer::tick() {
    if(m_heap.empty()) {
        return;
    }
    // 遍历时间堆的客户定时器，如果客户超时了，需要删掉
    while(!m_heap.empty()) {
        TimerNode node = m_heap.front();
        if(std::chrono::duration_cast<MS>(node.expires - Clock::now()).count() > 0) { 
            // 不超时
            break; 
        }
        node.cb();  
        pop();      // 删除根节点(如果根节点不超时，都不超时了)
    }
}


void HeapTimer::pop() {
    assert(!m_heap.empty());
    _del(0);
}

// 情空时间堆和fd和节点索引的键值对
void HeapTimer::clear() {
    m_ref.clear();
    m_heap.clear();
}

// 在清除超时节点后的时间堆里获取根节点，如果获取的根节点也超时返回0，要么得到其剩余的超时时间
int HeapTimer::GetNextTick() {
    tick();             // 清除超时结点
    size_t res = -1;
    if(!m_heap.empty()) {
        res = std::chrono::duration_cast<MS>(m_heap.front().expires - Clock::now()).count();
        if(res < 0) { res = 0; }
    }
    return res;
}

