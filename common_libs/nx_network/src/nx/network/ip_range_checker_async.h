#pragma once

#include <set>

#include <QtCore/QObject>
#include <QtNetwork/QHostAddress>

#include <nx/utils/thread/joinable.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

#include "http/asynchttpclient.h"
#include "nx/utils/thread/stoppable.h"

// TODO: #ak Inherit from aio::BasicPollable.

/**
 * Asynchronously scans specified ip address range for specified port to be opened and listening.
 */
class NX_NETWORK_API QnIpRangeCheckerAsync:
    public QObject,
    public QnStoppable,
    public QnJoinable
{
    Q_OBJECT

public:
    QnIpRangeCheckerAsync();
    ~QnIpRangeCheckerAsync();

    virtual void pleaseStop() override;
    virtual void join() override;

    /**
     * Asynchronously scans specified ip address range for portToScan to be opened and listening.
     * @return List of ip address from [startAddr; endAddr] range which have portToScan opened and listening.
     */
    QStringList onlineHosts(
        const QHostAddress& startAddr,
        const QHostAddress& endAddr,
        int portToScan);
    size_t hostsChecked() const;

    static int maxHostsCheckedSimultaneously();

private:
    bool m_terminated;
    QStringList m_openedIPs;
    // TODO: #ak Replace with nx_http::AsyncClient.
    std::set<nx_http::AsyncHttpClientPtr> m_socketsBeingScanned;
    mutable QnMutex m_mutex;
    QnWaitCondition m_cond;
    int m_portToScan;
    quint32 m_startIpv4;
    quint32 m_endIpv4;
    quint32 m_nextIPToCheck;

    bool launchHostCheck();

    private slots:
    void onDone(nx_http::AsyncHttpClientPtr);
};
