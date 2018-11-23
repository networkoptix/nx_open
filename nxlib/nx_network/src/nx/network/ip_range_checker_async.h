#pragma once

#include <set>

#include <QtCore/QObject>
#include <QtNetwork/QHostAddress>

#include <nx/utils/thread/mutex.h>
#include <nx/network/async_stoppable.h>
#include <nx/network/aio/basic_pollable.h>

#include "http/http_async_client.h"

// TODO: Migrate to HostAddr instead of strings...
// TODO: Remove Qt from here.

/**
 * Asynchronously scans specified ip address range for specified port to be opened and listening.
 */
class NX_NETWORK_API QnIpRangeScannerAsync:
    public nx::network::QnStoppableAsync
{
public:
    using CompletionHandler = nx::utils::MoveOnlyFunc<void(QStringList)>;

    QnIpRangeScannerAsync(nx::network::aio::AbstractAioThread* aioThread);
    virtual ~QnIpRangeScannerAsync() override;

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler) override;

    /**
     * Asynchronously scans specified ip address range for portToScan to be opened and listening.
     * @return List of ip address from [startAddr; endAddr] range which have portToScan opened and listening.
     */
    void scanOnlineHosts(
        CompletionHandler callback,
        const QHostAddress& startAddr,
        const QHostAddress& endAddr,
        int portToScan);

    static int maxHostsCheckedSimultaneously();
    size_t hostsChecked() const;

private:
    using Requests = std::set<std::unique_ptr<nx::network::http::AsyncClient>>;

    nx::network::aio::BasicPollable m_pollable;

    CompletionHandler m_completionHandler;
    bool m_terminated;
    QStringList m_onlineHosts;
    Requests m_socketsBeingScanned;
    int m_portToScan;
    quint32 m_startIpv4;
    quint32 m_endIpv4;
    quint32 m_nextIPToCheck;
    std::atomic<size_t> m_hostsChecked = 0;

    bool startHostScan();
    void onDone(Requests::iterator requestIter);
};
