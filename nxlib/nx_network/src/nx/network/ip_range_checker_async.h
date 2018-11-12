#pragma once

#include <set>

#include <QtCore/QObject>
#include <QtNetwork/QHostAddress>

#include <nx/utils/thread/joinable.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/utils/thread/stoppable.h>

#include "http/http_async_client.h"

// TODO: #ak Inherit from aio::BasicPollable.

/**
 * Asynchronously scans specified ip address range for specified port to be opened and listening.
 */
class NX_NETWORK_API QnIpRangeCheckerAsync:
    public QnStoppable,
    public QnJoinable
{
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
    using Requests = std::set<std::unique_ptr<nx::network::http::AsyncClient>>;

    bool m_terminated;
    QStringList m_openedIPs;
    Requests m_socketsBeingScanned;
    mutable QnMutex m_mutex;
    QnWaitCondition m_cond;
    int m_portToScan;
    quint32 m_startIpv4;
    quint32 m_endIpv4;
    quint32 m_nextIPToCheck;

    bool launchHostCheck();
    void onDone(Requests::iterator requestIter);
};
