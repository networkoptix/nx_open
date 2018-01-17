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
}

QnIpRangeCheckerAsync::~QnIpRangeCheckerAsync()
{
    pleaseStop();
    join();
}

void QnIpRangeCheckerAsync::pleaseStop()
{
    {
        QnMutexLocker lk(&m_mutex);
        m_terminated = true;
    }
}

void QnIpRangeCheckerAsync::join()
{
    QnMutexLocker lk(&m_mutex);
    while (!m_socketsBeingScanned.empty())
        m_cond.wait(lk.mutex());
}

QStringList QnIpRangeCheckerAsync::onlineHosts(
    const QHostAddress& startAddr,
    const QHostAddress& endAddr,
    int portToScan)
{
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
        return false;  //all ip addresses are being scanned at the moment
    quint32 ipToCheck = m_nextIPToCheck++;

    nx::network::http::AsyncHttpClientPtr httpClient = nx::network::http::AsyncHttpClient::create();
    connect(
        httpClient.get(), &nx::network::http::AsyncHttpClient::responseReceived,
        this, &QnIpRangeCheckerAsync::onDone,
        Qt::DirectConnection);
    connect(
        httpClient.get(), &nx::network::http::AsyncHttpClient::done,
        this, &QnIpRangeCheckerAsync::onDone,
        Qt::DirectConnection);
    httpClient->doGet(nx::utils::Url(lit("http://%1:%2/").arg(QHostAddress(ipToCheck).toString()).arg(m_portToScan)));
    m_socketsBeingScanned.insert(httpClient);
    return true;
}

void QnIpRangeCheckerAsync::onDone(nx::network::http::AsyncHttpClientPtr httpClient)
{
    QnMutexLocker lk(&m_mutex);

    std::set<nx::network::http::AsyncHttpClientPtr>::iterator it = m_socketsBeingScanned.find(httpClient);
    NX_ASSERT(it != m_socketsBeingScanned.end());
    if (httpClient->bytesRead() > 0)
        m_openedIPs.push_back(httpClient->url().host());

    httpClient->pleaseStopSync();
    m_socketsBeingScanned.erase(it);

    launchHostCheck();

    if (m_socketsBeingScanned.empty())
        m_cond.wakeAll();
}
