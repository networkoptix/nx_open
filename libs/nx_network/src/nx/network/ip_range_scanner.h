#pragma once

#include <unordered_set>

#include <nx/network/async_stoppable.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/http/http_async_client.h>

namespace nx::network {

/**
 * Asynchronously scans specified ip address range for specified port to be opened and listening.
 */
class NX_NETWORK_API IpRangeScanner:
    public aio::BasicPollable
{
    using base_type = aio::BasicPollable;

public:
    using CompletionHandler = nx::utils::MoveOnlyFunc<void(std::vector<nx::network::HostAddress>)>;

    IpRangeScanner(aio::AbstractAioThread* aioThread = nullptr);

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

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
    enum class State {readyToScan, scanning, terminated};
    using IpCheckers = std::unordered_set<std::unique_ptr<nx::network::http::AsyncClient>>;

    bool startHostCheck();
    void onDone(IpCheckers::iterator clientIter);
    virtual void stopWhileInAioThread() override;

private:
    CompletionHandler m_completionHandler;
    std::vector<nx::network::HostAddress> m_onlineHosts;
    IpCheckers m_ipCheckers;

    std::atomic<State> m_state = State::readyToScan;

    int m_portToScan = 0;
    uint32_t m_startIpv4 = 0;
    uint32_t m_endIpv4 = 0;
    uint32_t m_nextIPToCheck = 0;
    std::atomic<size_t> m_hostsChecked = 0;
};

} // namespace nx::network
