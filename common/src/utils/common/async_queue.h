#ifndef ASYNC_queueH
#define ASYNC_queueH

#include <QtCore/QThread>
#include <QtCore/QMutexLocker>
#include <QtCore/QQueue>
#include <QtCore/QMutex>
#include <QtCore/QWaitCondition>

template<class T>
class QnAsyncQueue
{
public:
    class Empty : public std::exception {
    };

    QnAsyncQueue(uint max = -1, unsigned long timeout = ULONG_MAX)
        : _max(max),
          _timeout(timeout),
          _mutex(),
          _condition(&_mutex) {
    }

    ~QnAsyncQueue() {
        clean();
    }

    uint count() {
        _mutex.lock();
        int count = _queue.count();
        _mutex.unlock();
        return count;
    }

    bool isFull() {
        if (-1 == _max)
            return false;

        _mutex.lock();
        int count = _queue.count();
        _mutex.unlock();
        return count >= max_;
    }

    bool isEmpty() {
        _mutex.lock();
        bool empty = _queue.isEmpty();
        _mutex.unlock();
        return empty;
    }

    void clean() {
        _mutex.lock();
        _queue.clear();
        _mutex.unlock();
    }

    void push(const T& t) {
        _mutex.lock();
        _queue.enqueue(t);
        _condition.wakeOne();
        _mutex.unlock();
    }

    T pull() {
        _mutex.lock();
        T i = _queue.dequeue();
        _mutex.unlock();
        return i;
    }

    T wait() throw (Empty) {
        QMutexLocker locker(&_mutex);

        while (_queue.isEmpty())
            _condition.wait(&_mutex, _timeout);

        if (_queue.isEmpty())
            throw Empty();

        return _queue.dequeue();
    }

private:
    int _max;
    unsigned long _timeout;

    QQueue<T> _queue;
    QMutex _mutex;
    QWaitCondition _condition;
};

#endif