/**********************************************************
* 10 jul 2014
* a.kolesnikov
***********************************************************/

#ifndef DAYTIME_NIST_FETCHER_H
#define DAYTIME_NIST_FETCHER_H

#include <QtCore/QByteArray>
#include <QtCore/QSharedPointer>

#include "abstract_accurate_time_fetcher.h"
#include "utils/network/aio/aioservice.h"


//!Fetches time from NIST servers using Daytime (rfc867) protocol
/*!
    Result time is accurate to second boundary only
*/
class DaytimeNISTFetcher
:
    public AbstractAccurateTimeFetcher,
    public aio::AIOEventHandler
{
public:
    DaytimeNISTFetcher();
    virtual ~DaytimeNISTFetcher();

    //!Implementation of \a QnStoppable::pleaseStop
    virtual void pleaseStop() override;
    //!Implementation of \a QnJoinable::join
    virtual void join() override;

    //!Implementation of \a AbstractAccurateTimeFetcher::getTimeAsync
    virtual bool getTimeAsync( std::function<void(qint64, SystemError::ErrorCode)> handlerFunc ) override;

private:
    QSharedPointer<AbstractStreamSocket> m_tcpSock;
    QByteArray m_timeStr;
    std::function<void(qint64, SystemError::ErrorCode)> m_handlerFunc;
    int m_readBufferSize;

    virtual void eventTriggered( AbstractSocket* sock, aio::EventType eventType ) throw() override;
};

#endif  //DAYTIME_NIST_FETCHER_H
