/**********************************************************
* 16 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef CACHED_OUTPUT_STREAM_H
#define CACHED_OUTPUT_STREAM_H

#include <queue>

#include <QtCore/QByteArray>
#include <nx/utils/thread/mutex.h>

#include <nx/utils/thread/long_runnable.h>
#include <utils/common/threadqueue.h>


class QnTCPConnectionProcessor;

//!Sends input data asynchronously using QnTCPConnectionProcessor::sendBuffer
class CachedOutputStream
:
    public QnLongRunnable
{
public:
    /*!
        \param ssl pointer to SSL. If not NULL, ssl is used
    */
    CachedOutputStream( QnTCPConnectionProcessor* const tcpOutput );
    ~CachedOutputStream();

    //! blocks if m_packetsToSend.size() > maxQueueSize
    void postPacket(const QByteArray& data, int maxQueueSize = -1);
    bool failed() const;
    size_t packetsInQueue() const;

    virtual void pleaseStop();

protected:
    virtual void run() override;

private:
    QnTCPConnectionProcessor* const m_tcpOutput;
    QnSafeQueue<QByteArray> m_packetsToSend;
    bool m_failed;
    mutable QnMutex m_mutex;
    QnWaitCondition m_cond;
};

#endif  //CACHED_OUTPUT_STREAM_H
