/**********************************************************
* 28 oct 2013
* akolesnikov@networkoptix.com
***********************************************************/

#ifndef IP_RANGE_CHECKER_ASYNC_H
#define IP_RANGE_CHECKER_ASYNC_H

#include <set>

#include <QtCore/QMutex>
#include <QtCore/QObject>
#include <QtCore/QWaitCondition>
#include <QtNetwork/QHostAddress>

#include "http/asynchttpclient.h"
#include "../common/stoppable.h"


//!Asynchronously scans specified ip address range for specified port to be opened and listening
class QnIprangeCheckerAsync
:
    public QObject,
    public QnStoppable
{
    Q_OBJECT

public:
    QnIprangeCheckerAsync();
    ~QnIprangeCheckerAsync();

    //!Implmmentation of \a QnStoppable::pleaseStop
    virtual void pleaseStop() override;

    //!Asynchronously scans specified ip address range for \a portToScan to be opened and listening
    /*!
        \return List of ip address from [\a startAddr; \a endAddr] range which have \a portToScan opened and listening
    */
    QList<QString> onlineHosts( const QHostAddress& startAddr, const QHostAddress& endAddr, int portToScan );

private:
    QList<QString> m_openedIPs;
    //!It is only because of aio::AsyncHttpClient API bug we have to create this terrible dictionary. it will be fixed soon
    std::set<std::shared_ptr<nx_http::AsyncHttpClient> > m_socketsBeingScanned;
    QMutex m_mutex;
    QWaitCondition m_cond;
    int m_portToScan;
    quint32 m_endIpv4;
    quint32 m_nextIPToCheck;

    //!Returns immediately if no scan is running
    void waitForScanToFinish();
    bool launchHostCheck();

private slots:
    void onDone( nx_http::AsyncHttpClientPtr );
};

#endif //IP_RANGE_CHECKER_ASYNC_H
