
#pragma once

#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

template<typename Request>
class QnMultiserverRequestContext
{
public:
    QnMultiserverRequestContext(const Request &request
        , int ownerPort);

    void incRequestsCount();

    void requestProcessed();

    void waitForDone();

    const Request &request() const;

    int ownerPort() const;

    typedef std::function<void ()> Handler;
    void executeGuarded(const Handler &handler);

private:
    const int m_ownerPort;
    int m_requestsInProgress;
    QnMutex m_mutex;
    QnWaitCondition m_waitCond;

    Request m_request;
};

/*
    Implementation
*/

template<typename Request>
QnMultiserverRequestContext<Request>::QnMultiserverRequestContext(const Request &initRequest
    , int ownerPort)

    : m_ownerPort(ownerPort)
    , m_requestsInProgress(0)
    , m_mutex()
    , m_waitCond()

    , m_request(initRequest)
{}

template<typename Request>
void QnMultiserverRequestContext<Request>::waitForDone()
{
    QnMutexLocker lock(&m_mutex);

    while (m_requestsInProgress > 0)
        m_waitCond.wait(&m_mutex);
}

template<typename Request>
void QnMultiserverRequestContext<Request>::requestProcessed()
{
    --m_requestsInProgress;
    m_waitCond.wakeAll();
}

template<typename Request>
void QnMultiserverRequestContext<Request>::incRequestsCount()
{
    ++m_requestsInProgress;
}

template<typename Request>
const Request &QnMultiserverRequestContext<Request>::request() const
{
    return m_request;
}

template<typename Request>
int QnMultiserverRequestContext<Request>::ownerPort() const
{
    return m_ownerPort;
}

template<typename Request>
void QnMultiserverRequestContext<Request>::executeGuarded(const Handler &handler)
{
    const QnMutexLocker guard(&m_mutex);
    handler();
}
