#include "ip_range_checker_async.h"

#include <nx/utils/log/log.h>

#include "socket.h"

static const int kMaxHostsCheckedSimultaneously = 256;

QnIpRangeScannerAsync::QnIpRangeScannerAsync(nx::network::aio::AbstractAioThread *aioThread):
    m_pollable(aioThread),
    m_terminated(false),
    m_portToScan(0),
    m_startIpv4(0),
    m_endIpv4(0),
    m_nextIPToCheck(0)
{
    NX_VERBOSE(this, "Created");
}

QnIpRangeScannerAsync::~QnIpRangeScannerAsync()
{
    pleaseStopSync();
}

void QnIpRangeScannerAsync::pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    m_pollable.dispatch(
        [this, completionHandler = std::move(completionHandler)]() mutable
        {
            NX_VERBOSE(this, "Terminate requested, range [%1, %2]",
                QHostAddress(m_startIpv4), QHostAddress(m_endIpv4));

            for (auto& httpClientPtr: m_socketsBeingScanned)
                httpClientPtr->pleaseStopSync();

            m_terminated = true;
            completionHandler();
        });
}


void QnIpRangeScannerAsync::scanOnlineHosts(
    CompletionHandler callback,
    const QHostAddress& startAddr,
    const QHostAddress& endAddr,
    int portToScan)
{
    m_pollable.dispatch(
        [=, callback = std::move(callback)]() mutable
        {
            NX_VERBOSE(this, "Starting search in range [%1, %2]", startAddr, endAddr);
            m_onlineHosts.clear();

            m_completionHandler = std::move(callback);
            m_portToScan = portToScan;
            m_startIpv4 = startAddr.toIPv4Address();
            m_endIpv4 = endAddr.toIPv4Address();
            NX_ASSERT(m_endIpv4 >= m_startIpv4);

            m_nextIPToCheck = m_startIpv4;
            for (int i = 0; i < kMaxHostsCheckedSimultaneously; ++i)
                startHostScan();
        });
}

size_t QnIpRangeScannerAsync::hostsChecked() const
{
    return m_hostsChecked;
}

int QnIpRangeScannerAsync::maxHostsCheckedSimultaneously()
{
    return kMaxHostsCheckedSimultaneously;
}

bool QnIpRangeScannerAsync::startHostScan()
{
    NX_ASSERT(m_pollable.isInSelfAioThread());
    NX_ASSERT(!m_terminated);

    if (m_nextIPToCheck > m_endIpv4)
        return false;  // All ip addresses are being scanned at the moment.
    quint32 ipToCheck = m_nextIPToCheck++;
    NX_VERBOSE(this, "Checking IP: %1", QHostAddress(ipToCheck));

    auto httpClientIter = m_socketsBeingScanned.insert(
        std::make_unique<nx::network::http::AsyncClient>()).first;
    (*httpClientIter)->bindToAioThread(m_pollable.getAioThread());
    (*httpClientIter)->setOnResponseReceived(
        std::bind(&QnIpRangeScannerAsync::onDone, this, httpClientIter));
    (*httpClientIter)->setOnDone(
        std::bind(&QnIpRangeScannerAsync::onDone, this, httpClientIter));

    (*httpClientIter)->setMaxNumberOfRedirects(0);
    (*httpClientIter)->doGet(nx::utils::Url(
        QString("http://%1:%2/").arg(QHostAddress(ipToCheck).toString()).arg(m_portToScan)));
    return true;
}

void QnIpRangeScannerAsync::onDone(Requests::iterator httpClientIter)
{
    NX_ASSERT(m_pollable.isInSelfAioThread());
    NX_ASSERT(!m_terminated);
    NX_ASSERT(httpClientIter != m_socketsBeingScanned.end());

    m_hostsChecked++;

    const auto host = (*httpClientIter)->url().host();
    if ((*httpClientIter)->bytesRead() > 0)
    {
        m_onlineHosts.push_back(host);
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
        QHostAddress(m_startIpv4), QHostAddress(m_endIpv4), m_onlineHosts.size(), m_terminated);

    nx::utils::moveAndCall(m_completionHandler, std::move(m_onlineHosts));
}
