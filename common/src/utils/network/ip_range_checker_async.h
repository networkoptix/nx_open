/**********************************************************
* 28 oct 2013
* akolesnikov@networkoptix.com
***********************************************************/

#ifndef IP_RANGE_CHECKER_ASYNC_H
#define IP_RANGE_CHECKER_ASYNC_H

#include <map>

#include <QtCore/QMutex>
#include <QtCore/QObject>
#include <QtCore/QWaitCondition>
#include <QtNetwork/QHostAddress>

#include "http/asynchttpclient.h"
#include "../common/stoppable.h"


//!Asynchronously scans specified ip address range for specified port to be opened and listening
class QnIpRangeCheckerAsync
:
    public QObject,
    public QnStoppable
{
    Q_OBJECT

public:
    QnIpRangeCheckerAsync();
    ~QnIpRangeCheckerAsync();

    //!Implmmentation of \a QnStoppable::pleaseStop
    virtual void pleaseStop() override;

    //!Asynchronously scans specified ip address range for \a portToScan to be opened and listening
    /*!
        \return List of ip address from [\a startAddr; \a endAddr] range which have \a portToScan opened and listening
    */
    QStringList onlineHosts( const QHostAddress& startAddr, const QHostAddress& endAddr, int portToScan );

private:
    enum Status
    {
        sInit,
        sPortOpened
    };

    QStringList m_openedIPs;
    //!It is only because of aio::AIOService API bug we have to create this terrible dictionary. Gonna fix that in default
    std::map<nx_http::AsyncHttpClient*, Status> m_socketsBeingScanned;
    QMutex m_mutex;
    QWaitCondition m_cond;
    int m_portToScan;
    quint32 m_endIpv4;
    quint32 m_nextIPToCheck;

    //!Returns immediately if no scan is running
    void waitForScanToFinish();
    bool launchHostCheck();

private slots:
    void onTcpConnectionEstablished( nx_http::AsyncHttpClient* );
    void onResponseReceived( nx_http::AsyncHttpClient* );
    void onDone( nx_http::AsyncHttpClient* );
};

#endif //IP_RANGE_CHECKER_ASYNC_H
