/**********************************************************
* 10 jul 2014
* a.kolesnikov
***********************************************************/

#ifndef DAYTIME_NIST_FETCHER_H
#define DAYTIME_NIST_FETCHER_H

#include <memory>

#include <QtCore/QByteArray>

#include "abstract_accurate_time_fetcher.h"
#include "utils/network/abstract_socket.h"


//!Fetches time from NIST servers using Daytime (rfc867) protocol
/*!
    Result time is accurate to second boundary only
*/
class DaytimeNISTFetcher
:
    public AbstractAccurateTimeFetcher
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
    std::unique_ptr<AbstractStreamSocket> m_tcpSock;
    QByteArray m_timeStr;
    std::function<void(qint64, SystemError::ErrorCode)> m_handlerFunc;

    void onConnectionEstablished( SystemError::ErrorCode errorCode ) noexcept;
    void onSomeBytesRead( SystemError::ErrorCode errorCode, size_t bytesRead ) noexcept;
};

#endif  //DAYTIME_NIST_FETCHER_H
