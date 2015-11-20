/**********************************************************
* 10 jul 2014
* a.kolesnikov
***********************************************************/

#ifndef DAYTIME_NIST_FETCHER_H
#define DAYTIME_NIST_FETCHER_H

#include <memory>

#include <QtCore/QByteArray>
#include <utils/thread/mutex.h>

#include "abstract_accurate_time_fetcher.h"
#include <nx/network/abstract_socket.h>


//!Fetches time using Time (rfc868) protocol
/*!
    Result time is accurate to second boundary only
*/
class TimeProtocolClient
:
    public AbstractAccurateTimeFetcher
{
public:
    TimeProtocolClient( const QString& timeServer );
    virtual ~TimeProtocolClient();

    //!Implementation of \a QnStoppable::pleaseStop
    virtual void pleaseStop() override;
    //!Implementation of \a QnJoinable::join
    virtual void join() override;

    //!Implementation of \a AbstractAccurateTimeFetcher::getTimeAsync
    virtual void getTimeAsync( std::function<void(qint64, SystemError::ErrorCode)> handlerFunc ) override;

private:
    const QString m_timeServer;
    std::shared_ptr<AbstractStreamSocket> m_tcpSock;
    QByteArray m_timeStr;
    std::function<void(qint64, SystemError::ErrorCode)> m_handlerFunc;
    mutable QnMutex m_mutex;

    void onConnectionEstablished( SystemError::ErrorCode errorCode );
    void onSomeBytesRead( SystemError::ErrorCode errorCode, size_t bytesRead );
};

#endif  //DAYTIME_NIST_FETCHER_H
