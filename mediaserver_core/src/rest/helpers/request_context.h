
#pragma once

#include <utils/thread/mutex.h>

class QnRestConnectionProcessor;

template<typename RequestType>
struct QnMultiserverRequestContext
{
    QnMultiserverRequestContext(const RequestType &initRequest
        , const QnRestConnectionProcessor *initOwner);

    void waitForDone();

    QnMutex mutex;
    QnWaitCondition waitCond;
    int requestsInProgress;
    const QnRestConnectionProcessor *owner;

    RequestType request;
};

// Implementation

template<typename RequestType>
QnMultiserverRequestContext<RequestType>::QnMultiserverRequestContext(const RequestType &initRequest
    , const QnRestConnectionProcessor *initOwner)

    : mutex()
    , waitCond()
    , requestsInProgress(0)
    , owner(initOwner)

    , request(initRequest)
{}

template<typename RequestType>
void QnMultiserverRequestContext<RequestType>::waitForDone()
{
    QnMutexLocker lock(&mutex);

    while (requestsInProgress > 0)
        waitCond.wait(&mutex);
}

