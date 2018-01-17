#include "public_ip_discovery.h"

#include <chrono>

#include <QtCore/QCoreApplication>
#include <QtCore/QTime>

#include <nx/network/http/http_client.h>
#include <nx/utils/log/log.h>

namespace {

const QString kDefaultPrimaryUrlsList(QLatin1String("http://www.mypublicip.com;http://checkip.eurodyndns.org"));
const QString kDefaultSecondaryUrlsList(QLatin1String("http://networkoptix.com/myip"));
const int kRequestTimeoutMs = 4 * 1000;
const QLatin1String kIpRegExprValue("[^a-zA-Z0-9\\.](([0-9]){1,3}\\.){3}([0-9]){1,3}[^a-zA-Z0-9\\.]");

} // namespace

namespace nx {
namespace network {

PublicIPDiscovery::PublicIPDiscovery(QStringList primaryUrls):
    m_stage(Stage::idle),
    m_replyInProgress(0),
    m_primaryUrls(std::move(primaryUrls))
{
    if (m_primaryUrls.isEmpty())
        m_primaryUrls = kDefaultPrimaryUrlsList.split(lit(";"), QString::SkipEmptyParts);
    m_secondaryUrls = kDefaultSecondaryUrlsList.split(lit(";"), QString::SkipEmptyParts);

    if (m_primaryUrls.isEmpty())
    {
        m_primaryUrls = m_secondaryUrls;
        m_secondaryUrls.clear();
    }

    NX_LOG(lit("Primary urls: %1").arg(m_primaryUrls.join(lit("; "))), cl_logDEBUG2);
    NX_LOG(lit("Secondary urls: %1").arg(m_secondaryUrls.join(lit("; "))), cl_logDEBUG2);

    NX_ASSERT(
        !m_primaryUrls.isEmpty(),
        Q_FUNC_INFO,
        "Server should have at least one public IP url");
}

PublicIPDiscovery::~PublicIPDiscovery()
{
    pleaseStopSync();
}

void PublicIPDiscovery::bindToAioThread(
    nx::network::aio::AbstractAioThread* aioThread)
{
    nx::network::aio::BasicPollable::bindToAioThread(aioThread);
    for (auto& httpRequest: m_httpRequests)
        httpRequest->bindToAioThread(aioThread);
}

void PublicIPDiscovery::update()
{
    m_stage = Stage::primaryUrlsRequesting;
    for (const QString &url : m_primaryUrls)
        sendRequest(url);
}

void PublicIPDiscovery::waitForFinished()
{
    auto waitFunc =
        [this](int timeout)
        {
            QTime t;
            t.start();
            while (t.elapsed() < timeout && m_publicIP.isNull() && m_replyInProgress > 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                qApp->processEvents();
            }
        };

    waitFunc(kRequestTimeoutMs);

    /* If no public ip found from primary servers, send requests to secondary servers (if any). */
    if (m_stage == Stage::primaryUrlsRequesting)
        nextStage();

    /* Give additional timeout for secondary servers. */
    if (m_stage == Stage::secondaryUrlsRequesting)
    {
        NX_LOG(lit("Giving additional timeout"), cl_logDEBUG2);
        waitFunc(kRequestTimeoutMs);
    }
}

QHostAddress PublicIPDiscovery::publicIP() const
{
    return m_publicIP;
}

void PublicIPDiscovery::handleReply(const nx::network::http::AsyncHttpClientPtr& httpClient)
{
    /* Check if reply finished successfully. */

    if ((httpClient->failed()) ||
        (httpClient->response()->statusLine.statusCode != nx::network::http::StatusCode::ok))
    {
        return;
    }

    /* QRegExp class is not thread-safe. */
    const QRegExp ipRegExpr(kIpRegExprValue);

    /* Check if reply contents contain any ip address. */
    QByteArray response =
        QByteArray(" ") + httpClient->fetchMessageBodyBuffer() + QByteArray(" ");
    const int ipPos = ipRegExpr.indexIn(QString::fromUtf8(response));
    if (ipPos < 0)
        return;

    QString result = QString::fromLatin1(
        response.mid(ipPos + 1, ipRegExpr.matchedLength() - 2));
    if (result.isEmpty())
        return;

    QHostAddress newAddress = QHostAddress(result);
    if (!newAddress.isNull())
    {
        m_stage = Stage::publicIpFound;
        NX_LOG(lit("Set stage to %1").arg(toString(m_stage)), cl_logDEBUG2);
        if (newAddress != m_publicIP)
        {
            m_publicIP = newAddress;
            emit found(m_publicIP);
        }
    }
}

void PublicIPDiscovery::sendRequest(const QString &url)
{
    nx::network::http::AsyncHttpClientPtr httpRequest = nx::network::http::AsyncHttpClient::create();
    httpRequest->bindToAioThread(getAioThread());
    {
        QnMutexLocker lock(&m_mutex);
        m_httpRequests.insert(httpRequest);
    }

    auto at_reply_finished =
        [this](const nx::network::http::AsyncHttpClientPtr& httpClient) mutable
        {
            handleReply(httpClient);
            httpClient->disconnect();
            m_replyInProgress--;
            if (m_replyInProgress == 0)
                nextStage();

            /* If no public ip found, clean existing. */
            if (m_stage == Stage::idle && !m_publicIP.isNull())
            {
                m_publicIP.clear();
                emit found(m_publicIP);
            }

            QnMutexLocker lock(&m_mutex);
            m_httpRequests.erase(httpClient);
        };

    NX_LOG(lit("Sending request to %1").arg(url), cl_logDEBUG2);
    m_replyInProgress++;

    httpRequest->setResponseReadTimeoutMs(kRequestTimeoutMs);
    httpRequest->doGet(url, at_reply_finished);
}

void PublicIPDiscovery::nextStage()
{
    NX_LOG(lit("Next stage from %1").arg(toString(m_stage)), cl_logDEBUG2);
    if (m_stage == Stage::publicIpFound)
        return;

    if (m_stage == Stage::primaryUrlsRequesting && !m_secondaryUrls.isEmpty())
    {
        m_stage = Stage::secondaryUrlsRequesting;
        NX_LOG(lit("Set stage to %1").arg(toString(m_stage)), cl_logDEBUG2);
        for (const QString &url : m_secondaryUrls)
            sendRequest(url);
    }
    else
    {
        m_stage = Stage::idle;
        NX_LOG(lit("Set stage to %1").arg(toString(m_stage)), cl_logDEBUG2);
    }
}

void PublicIPDiscovery::stopWhileInAioThread()
{
    m_httpRequests.clear();
}

QString PublicIPDiscovery::toString(Stage value) const
{
    switch (value)
    {
        case Stage::idle:
            return lit("idle");
        case Stage::primaryUrlsRequesting:
            return lit("primaryUrlsRequesting");
        case Stage::secondaryUrlsRequesting:
            return lit("secondaryUrlsRequesting");
        case Stage::publicIpFound:
            return lit("publicIpFound");
        default:
            NX_ASSERT(false);
            return QString();
    }
}

} // namespace network
} // namespace nx
