#include "ip_range_scanner.h"

#include <nx/network/url/url_builder.h>
#include <nx/utils/log/log.h>

namespace {

constexpr int kMaxHostsCheckedSimultaneously = 256;

} // namespace

namespace nx::network {

IpRangeScanner::IpRangeScanner(nx::network::aio::AbstractAioThread *aioThread):
    m_pollable(aioThread)
{
    NX_VERBOSE(this, "Created");
}

IpRangeScanner::~IpRangeScanner()
{
    pleaseStopSync();
}

void IpRangeScanner::pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    m_pollable.dispatch(
        [this, completionHandler = std::move(completionHandler)]() mutable
        {
            NX_VERBOSE(this, "Terminate requested, range [%1, %2]",
                nx::network::HostAddress(nx::network::HostAddress::ipV4from(m_startIpv4)),
                nx::network::HostAddress(nx::network::HostAddress::ipV4from(m_endIpv4)));

            for (auto& httpClientPtr: m_socketsBeingScanned)
                httpClientPtr->pleaseStopSync();

            m_terminated = true;
            completionHandler();
        });
}


void IpRangeScanner::scanOnlineHosts(
    CompletionHandler callback,
    nx::network::HostAddress startAddr,
    nx::network::HostAddress endAddr,
    int portToScan)
{
    NX_ASSERT(startAddr.isIpAddress());
    NX_ASSERT(endAddr.isIpAddress());

    m_pollable.dispatch(
        [this, startAddr, endAddr, portToScan, callback = std::move(callback)]() mutable
        {
            NX_VERBOSE(this, "Starting search in range [%1, %2]", startAddr, endAddr);
            m_onlineHosts.clear();

            m_completionHandler = std::move(callback);
            m_portToScan = portToScan;
            m_startIpv4 = ntohl(startAddr.ipV4().get().s_addr);
            m_endIpv4 = ntohl(endAddr.ipV4().get().s_addr);
            NX_ASSERT(m_endIpv4 >= m_startIpv4);

            m_nextIPToCheck = m_startIpv4;
            for (int i = 0; i < kMaxHostsCheckedSimultaneously; ++i)
                startHostScan();
        });
}

size_t IpRangeScanner::hostsChecked() const
{
    return m_hostsChecked;
}

int IpRangeScanner::maxHostsCheckedSimultaneously()
{
    return kMaxHostsCheckedSimultaneously;
}

bool IpRangeScanner::startHostScan()
{
    NX_ASSERT(m_pollable.isInSelfAioThread());
    NX_ASSERT(!m_terminated);

    if (m_nextIPToCheck > m_endIpv4)
        return false;  // All ip addresses are being scanned at the moment.
    uint32_t ipToCheck = m_nextIPToCheck++;
    NX_VERBOSE(this, "Checking IP: %1",
        nx::network::HostAddress(nx::network::HostAddress::ipV4from(ipToCheck)));

    auto httpClientIter = m_socketsBeingScanned.insert(
        std::make_unique<nx::network::http::AsyncClient>()).first;
    (*httpClientIter)->bindToAioThread(m_pollable.getAioThread());
    (*httpClientIter)->setOnResponseReceived(
        std::bind(&IpRangeScanner::onDone, this, httpClientIter));
    (*httpClientIter)->setOnDone(
        std::bind(&IpRangeScanner::onDone, this, httpClientIter));

    (*httpClientIter)->setMaxNumberOfRedirects(0);
    (*httpClientIter)->doGet(nx::network::url::Builder()
        .setScheme(nx::network::http::kUrlSchemeName)
        .setEndpoint({nx::network::HostAddress(nx::network::HostAddress::ipV4from(ipToCheck)),
            uint16_t(m_portToScan)})
        .toUrl());
    return true;
}

void IpRangeScanner::onDone(Requests::iterator httpClientIter)
{
    NX_ASSERT(m_pollable.isInSelfAioThread());
    NX_ASSERT(!m_terminated);
    NX_ASSERT(httpClientIter != m_socketsBeingScanned.end());

    m_hostsChecked++;

    const auto host = (*httpClientIter)->url().host();
    if ((*httpClientIter)->bytesRead() > 0)
    {
        m_onlineHosts.push_back((*httpClientIter)->socket()->getForeignAddress().address);
        NX_VERBOSE(this, "Checked IP: %1 (online)", host);
    }
    else
    {
        NX_VERBOSE(this, "Checked IP: %1 (offline)", host);
    }

    m_socketsBeingScanned.erase(httpClientIter);

    startHostScan();
    if (!m_socketsBeingScanned.empty()) // Check if it was the last host check.
        return;

    NX_VERBOSE(this, "Search in range [%1, %2] has finished, %3 hosts are online (terminated: %4)",
        nx::network::HostAddress(nx::network::HostAddress::ipV4from(m_startIpv4)),
        nx::network::HostAddress(nx::network::HostAddress::ipV4from(m_endIpv4)),
        m_onlineHosts.size(), m_terminated);

    nx::utils::moveAndCall(m_completionHandler, std::move(m_onlineHosts));
}

} // namespace nx::network
