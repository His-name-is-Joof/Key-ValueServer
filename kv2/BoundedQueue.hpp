#ifndef EPOCHLABS_BOUNDEDQUEUE_HPP
#define EPOCHLABS_BOUNDEDQUEUE_HPP

#include <mutex>
#include <condition_variable>
#include <queue>

namespace EpochLabsTest {

class Semaphore {
public:
    Semaphore(int _count = 0) : count(_count) {};
    void wait();
    void signal();
private:
    std::mutex mtx;
    std::condition_variable cv;
    int count;
};

// Threadsafe queue bounded by a semaphore and locked with mutexes while it's being manipulated
template <typename T>
class BoundedQueue {
public:
    // Locks the queue and pushes an element
    void push(T elem) {
        std::unique_lock<std::mutex> qlock(qmtx);
        qlock.lock();
        q.push(elem);
        qlock.unlock();
        sem.signal();
    };

    // Atomically removes front element and returns it's value
    T pop() {
        sem.wait();
        std::unique_lock<std::mutex> qlock(qmtx);
        qlock.lock();
        T elem = q.front();
        q.pop();
        qlock.unlock();
        return elem;
    };
private:
    std::queue<T> q;
    Semaphore sem;
    std::mutex qmtx;
};

}

#endif
