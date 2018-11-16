#include "ip_range_checker_async.h"

#include <nx/utils/log/log.h>

#include "socket.h"

static const int kMaxHostsCheckedSimultaneously = 256;

QnIpRangeCheckerAsync::QnIpRangeCheckerAsync():
    m_terminated(false),
    m_portToScan(0),
    m_startIpv4(0),
    m_endIpv4(0),
    m_nextIPToCheck(0)
{
    NX_VERBOSE(this, "Created");
}

QnIpRangeCheckerAsync::~QnIpRangeCheckerAsync()
{
    pleaseStop();
    join();
}

void QnIpRangeCheckerAsync::pleaseStop()
{
    NX_VERBOSE(this, "Terminate requested, range (%1, %2)!",
        QHostAddress(m_startIpv4), QHostAddress(m_endIpv4));
    {
        QnMutexLocker lk(&m_mutex);
        m_terminated = true;
    }
}

void QnIpRangeCheckerAsync::join()
{
    NX_VERBOSE(this, "Waiting to finish");
    QnMutexLocker lk(&m_mutex);
    while (!m_socketsBeingScanned.empty())
        m_cond.wait(lk.mutex());
    NX_VERBOSE(this, "Finished");
}

QStringList QnIpRangeCheckerAsync::onlineHosts(
    const QHostAddress& startAddr,
    const QHostAddress& endAddr,
    int portToScan)
{
    NX_VERBOSE(this, "Starting search in range (%1, %2)", startAddr, endAddr);
    {
        QnMutexLocker lk(&m_mutex);
        m_openedIPs.clear();

        m_portToScan = portToScan;
        m_startIpv4 = startAddr.toIPv4Address();
        m_endIpv4 = endAddr.toIPv4Address();

        if (m_endIpv4 < m_startIpv4)
            return QList<QString>();

        m_nextIPToCheck = m_startIpv4;
        for (int i = 0; i < kMaxHostsCheckedSimultaneously; ++i)
        {
            if (!launchHostCheck())
                break;
        }
    }

    join();

    NX_VERBOSE(this, "Search in range (%1, %2) has finished, %3 hosts are online",
        startAddr, endAddr, m_openedIPs.size());
    return m_openedIPs;
}

size_t QnIpRangeCheckerAsync::hostsChecked() const
{
    QnMutexLocker lk(&m_mutex);
    return (m_nextIPToCheck - m_startIpv4) - m_socketsBeingScanned.size();
}

int QnIpRangeCheckerAsync::maxHostsCheckedSimultaneously()
{
    return kMaxHostsCheckedSimultaneously;
}

bool QnIpRangeCheckerAsync::launchHostCheck()
{
    if (m_terminated)
        return false;

    if (m_nextIPToCheck > m_endIpv4)
        return false;  // All ip addresses are being scanned at the moment.
    quint32 ipToCheck = m_nextIPToCheck++;
    NX_VERBOSE(this, "Checking IP: %1", QHostAddress(ipToCheck));

    auto httpClientIter = m_socketsBeingScanned.insert(
        std::make_unique<nx::network::http::AsyncClient>()).first;
    (*httpClientIter)->setOnResponseReceived(
        std::bind(&QnIpRangeCheckerAsync::onDone, this, httpClientIter));
    (*httpClientIter)->setOnDone(
        std::bind(&QnIpRangeCheckerAsync::onDone, this, httpClientIter));

    (*httpClientIter)->setMaxNumberOfRedirects(0);
    (*httpClientIter)->doGet(nx::utils::Url(
        QString("http://%1:%2/").arg(QHostAddress(ipToCheck).toString()).arg(m_portToScan)));
    return true;
}

void QnIpRangeCheckerAsync::onDone(Requests::iterator httpClientIter)
{
    QnMutexLocker lk(&m_mutex);

    NX_ASSERT(httpClientIter != m_socketsBeingScanned.end());

    const auto host = (*httpClientIter)->url().host();
    if ((*httpClientIter)->bytesRead() > 0)
    {
        m_openedIPs.push_back(host);
        NX_VERBOSE(this, "Checked IP: %1 (online)", host);
    }
    else
    {
        NX_VERBOSE(this, "Checked IP: %1 (offline)", host);
    }

    m_socketsBeingScanned.erase(httpClientIter);

    launchHostCheck();

    if (m_socketsBeingScanned.empty())
        m_cond.wakeAll();
}
