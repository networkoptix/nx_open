#include "ip_range_scanner.h"

#include <nx/network/url/url_builder.h>
#include <nx/utils/log/log.h>

static constexpr int kMaxHostsCheckedSimultaneously = 256;

namespace nx::network {

IpRangeScanner::IpRangeScanner(aio::AbstractAioThread* aioThread)
{
    NX_VERBOSE(this, "Created");
    bindToAioThread(aioThread ? aioThread : getAioThread());
}

void IpRangeScanner::bindToAioThread(nx::network::aio::AbstractAioThread* aioThread)
{
    NX_ASSERT(m_state == State::readyToScan);
    base_type::bindToAioThread(aioThread);
}

void IpRangeScanner::stopWhileInAioThread()
{
    NX_VERBOSE(this, "Stop requested, range [%1, %2]",
        HostAddress(nx::network::HostAddress::ipV4from(m_startIpv4)),
        HostAddress(nx::network::HostAddress::ipV4from(m_endIpv4)));
    m_ipCheckers.clear();
    m_state = State::terminated;
}

void IpRangeScanner::scanOnlineHosts(
    CompletionHandler callback,
    nx::network::HostAddress startAddr,
    nx::network::HostAddress endAddr,
    int portToScan)
{
    NX_ASSERT(startAddr.isIpAddress());
    NX_ASSERT(endAddr.isIpAddress());

    dispatch(
        [this, startAddr, endAddr, portToScan, callback = std::move(callback)]() mutable
        {
            NX_ASSERT(m_state == State::readyToScan);
            NX_VERBOSE(this, "Starting search in range [%1, %2]", startAddr, endAddr);
            m_onlineHosts.clear();

            m_completionHandler = std::move(callback);
            m_portToScan = portToScan;
            m_startIpv4 = ntohl(startAddr.ipV4().get().s_addr);
            m_endIpv4 = ntohl(endAddr.ipV4().get().s_addr);
            NX_ASSERT(m_endIpv4 >= m_startIpv4);

            m_state = State::scanning;
            m_nextIPToCheck = m_startIpv4;
            for (int i = 0; i < kMaxHostsCheckedSimultaneously; ++i)
                startHostCheck();
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

bool IpRangeScanner::startHostCheck()
{
    NX_ASSERT(isInSelfAioThread());
    NX_ASSERT(m_state == State::scanning);

    if (m_nextIPToCheck > m_endIpv4)
        return false;  // All ip addresses are being scanned at the moment.
    uint32_t ipToCheck = m_nextIPToCheck++;
    NX_VERBOSE(this, "Checking IP: %1", HostAddress(HostAddress::ipV4from(ipToCheck)));

    auto clientIter = m_ipCheckers.insert(std::make_unique<http::AsyncClient>()).first;
    (*clientIter)->bindToAioThread(getAioThread());
    (*clientIter)->setOnResponseReceived(std::bind(&IpRangeScanner::onDone, this, clientIter));
    (*clientIter)->setOnDone(std::bind(&IpRangeScanner::onDone, this, clientIter));

    (*clientIter)->setMaxNumberOfRedirects(0);
    (*clientIter)->doGet(nx::network::url::Builder()
        .setScheme(nx::network::http::kUrlSchemeName)
        .setEndpoint({HostAddress(HostAddress::ipV4from(ipToCheck)), uint16_t(m_portToScan)})
        .toUrl());
    return true;
}

void IpRangeScanner::onDone(IpCheckers::iterator clientIter)
{
    NX_ASSERT(isInSelfAioThread());
    NX_ASSERT(m_state == State::scanning);
    NX_ASSERT(clientIter != m_ipCheckers.end());

    m_hostsChecked++;

    const auto host = (*clientIter)->url().host();
    if ((*clientIter)->bytesRead() > 0)
    {
        m_onlineHosts.push_back((*clientIter)->socket()->getForeignAddress().address);
        NX_VERBOSE(this, "Checked IP: %1 (online)", host);
    }
    else
    {
        NX_VERBOSE(this, "Checked IP: %1 (offline)", host);
    }

    m_ipCheckers.erase(clientIter);

    startHostCheck();
    if (!m_ipCheckers.empty()) // Check if it was the last host check.
        return;

    NX_VERBOSE(this, "Search in range [%1, %2] has finished, %3 hosts are online",
        HostAddress(HostAddress::ipV4from(m_startIpv4)),
        HostAddress(HostAddress::ipV4from(m_endIpv4)),
        m_onlineHosts.size());

    m_state = State::readyToScan;
    nx::utils::moveAndCall(m_completionHandler, std::move(m_onlineHosts));
}

} // namespace nx::network
