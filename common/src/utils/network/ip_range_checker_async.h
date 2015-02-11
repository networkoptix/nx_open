/**********************************************************
* 28 oct 2013
* akolesnikov@networkoptix.com
***********************************************************/

#ifndef IP_RANGE_CHECKER_ASYNC_H
#define IP_RANGE_CHECKER_ASYNC_H

#include <set>

#include <utils/common/mutex.h>
#include <QtCore/QObject>
#include <utils/common/wait_condition.h>
#include <QtNetwork/QHostAddress>

#include <utils/common/joinable.h>

#include "http/asynchttpclient.h"
#include "../common/stoppable.h"


//!Asynchronously scans specified ip address range for specified port to be opened and listening
class QnIpRangeCheckerAsync
:
    public QObject,
    public QnStoppable,
    public QnJoinable
{
    Q_OBJECT

public:
    QnIpRangeCheckerAsync();
    ~QnIpRangeCheckerAsync();

    //!Implmmentation of \a QnStoppable::pleaseStop
    virtual void pleaseStop() override;
    //!Implmmentation of \a QnJoinable::join
    virtual void join() override;

    //!Asynchronously scans specified ip address range for \a portToScan to be opened and listening
    /*!
        \return List of ip address from [\a startAddr; \a endAddr] range which have \a portToScan opened and listening
    */
    QStringList onlineHosts( const QHostAddress& startAddr, const QHostAddress& endAddr, int portToScan );
    size_t hostsChecked() const;

    static int maxHostsCheckedSimultaneously();

private:
    bool m_terminated;
    QStringList m_openedIPs;
    //!It is only because of aio::AsyncHttpClient API bug we have to create this terrible dictionary. it will be fixed soon
    std::set<nx_http::AsyncHttpClientPtr> m_socketsBeingScanned;
    mutable QnMutex m_mutex;
    QnWaitCondition m_cond;
    int m_portToScan;
    quint32 m_startIpv4;
    quint32 m_endIpv4;
    quint32 m_nextIPToCheck;

    bool launchHostCheck();

private slots:
    void onDone( nx_http::AsyncHttpClientPtr );
};

#endif //IP_RANGE_CHECKER_ASYNC_H
