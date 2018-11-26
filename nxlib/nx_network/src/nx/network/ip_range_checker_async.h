#pragma once

#include <set>

#include <nx/network/async_stoppable.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/http/http_async_client.h>

/**
 * Asynchronously scans specified ip address range for specified port to be opened and listening.
 */
class NX_NETWORK_API QnIpRangeScannerAsync:
    public nx::network::QnStoppableAsync
{
public:
    using CompletionHandler = nx::utils::MoveOnlyFunc<void(std::vector<nx::network::HostAddress>)>;

    QnIpRangeScannerAsync(nx::network::aio::AbstractAioThread* aioThread);
    virtual ~QnIpRangeScannerAsync() override;

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler) override;

    /**
     * Asynchronously scans specified ip address range for portToScan to be opened and listening.
     * @return List of ip address from [startAddr; endAddr] range which have portToScan opened and listening.
     */
    void scanOnlineHosts(
        CompletionHandler callback,
        nx::network::HostAddress startAddr,
        nx::network::HostAddress endAddr,
        int portToScan);

    static int maxHostsCheckedSimultaneously();
    size_t hostsChecked() const;

private:
    using Requests = std::set<std::unique_ptr<nx::network::http::AsyncClient>>;

    nx::network::aio::BasicPollable m_pollable;

    CompletionHandler m_completionHandler;
    std::vector<nx::network::HostAddress> m_onlineHosts;
    Requests m_socketsBeingScanned;

    bool m_terminated = false;

    int m_portToScan = 0;
    uint32_t m_startIpv4 = 0;
    uint32_t m_endIpv4 = 0;
    uint32_t m_nextIPToCheck = 0;
    std::atomic<size_t> m_hostsChecked = 0;

    bool startHostScan();
    void onDone(Requests::iterator requestIter);
};
