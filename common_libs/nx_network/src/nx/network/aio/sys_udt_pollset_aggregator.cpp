
#if 1

#include "sys_udt_pollset_aggregator.h"

#include <atomic>

#include <utils/common/long_runnable.h>


namespace nx {
namespace network {
namespace aio {

class UdtPollingThread
:
    public QnLongRunnable
{
public:
    UdtPollingThread(
        UdtPollSet* udtPollSet,
        PollSet* sysPollSet)
    :
        m_udtPollSet(udtPollSet),
        m_sysPollSet(sysPollSet),
        m_pollResult(0),
        m_state(State::waiting)
    {
        start();
    }

    ~UdtPollingThread()
    {
        {
            QnMutexLocker lk(&m_mutex);
            m_state = State::terminated;
            m_cond.wakeAll();
        }
        m_udtPollSet->interrupt();

        wait();
    }

    void doPoll()
    {
        QnMutexLocker lk(&m_mutex);
        m_state = State::polling;
        m_cond.wakeAll();
    }

    void stopPolling()
    {
        QnMutexLocker lk(&m_mutex);
        if (m_state != State::gotSomeEvents)
            m_udtPollSet->interrupt();
        m_state = State::waiting;
        m_cond.wakeAll();
    }

    int pollResult() const
    {
        QnMutexLocker lk(&m_mutex);
        while (m_state != State::gotSomeEvents)
            m_cond.wait(lk.mutex());    //TODO #ak this is a potential performance killer
        return m_pollResult;
    }

protected:
    virtual void run() override
    {
        QnMutexLocker lk(&m_mutex);
        for (;;)
        {
            while (m_state < State::polling)
                m_cond.wait(lk.mutex());
            if (m_state == State::terminated)
                break;
            lk.unlock();
            const int pollResult = m_udtPollSet->poll();
            lk.relock();
            if (m_state == State::terminated)
                break;
            m_pollResult = pollResult;
            m_state = State::gotSomeEvents;
            m_cond.wakeAll();
            if (m_pollResult > 0)
            {
                lk.unlock();
                m_sysPollSet->interrupt();
            }
        }
    }

private:
    enum class State
    {
        waiting,
        gotSomeEvents,
        polling,
        terminated
    };

    UdtPollSet* m_udtPollSet;
    PollSet* m_sysPollSet;
    int m_pollResult;
    mutable QnMutex m_mutex;
    State m_state;
};

PollSetAggregator::PollSetAggregator()
:
    m_udtPollingThread(new UdtPollingThread(&m_udtPollSet, &m_sysPollSet))
{
}

PollSetAggregator::~PollSetAggregator()
{
    delete m_udtPollingThread;
    m_udtPollingThread = nullptr;
}

bool PollSetAggregator::isValid() const
{
    return m_sysPollSet.isValid() && m_udtPollSet.isValid();
}

void PollSetAggregator::interrupt()
{
    m_sysPollSet.interrupt();
}

bool PollSetAggregator::add(
    Pollable* const sock,
    EventType eventType,
    void* userData)
{
    if (sock->isUdtSocket)
        return add(static_cast<UdtSocket*>(sock), eventType, userData);
    return m_sysPollSet.add(sock, eventType, userData);
}

void PollSetAggregator::remove(
    Pollable* const sock,
    EventType eventType)
{
    if (sock->isUdtSocket)
        return remove(static_cast<UdtSocket*>(sock), eventType);
    return m_sysPollSet.remove(sock, eventType);
}

size_t PollSetAggregator::size() const
{
    return m_sysPollSet.size() + m_udtPollSet.size();
}

int PollSetAggregator::poll(int millisToWait)
{
    m_udtPollingThread->doPoll();
    const int sysPollResult = m_sysPollSet.poll(millisToWait);
    m_udtPollingThread->stopPolling();
    const int udtPollResult = m_udtPollingThread->pollResult();

    //actual number returned does not matter. Only relation to zero matters
    if (sysPollResult != 0)
        return sysPollResult;

    return sysPollResult + udtPollResult;
}

PollSetAggregator::const_iterator PollSetAggregator::begin() const
{
}

PollSetAggregator::const_iterator PollSetAggregator::end() const
{
}

bool PollSetAggregator::add(
    UdtSocket* const sock,
    EventType eventType,
    void* userData)
{
    return m_udtPollSet.add(sock, eventType, userData);
}

void PollSetAggregator::remove(
    UdtSocket* const sock,
    EventType eventType)
{
    return m_udtPollSet.remove(sock, eventType);
}

}   //aio
}   //network
}   //nx

#endif
