
#include "public_ip_discovery.h"

#include <QtCore/QCoreApplication>

#include <nx/network/http/httpclient.h>
#include <nx/utils/log/log.h>
#include <utils/common/sleep.h>


namespace {

const QString kDefaultPrimaryUrlsList(QLatin1String("http://www.mypublicip.com;http://checkip.eurodyndns.org"));
const QString kDefaultSecondaryUrlsList(QLatin1String("http://networkoptix.com/myip"));
const int kRequestTimeoutMs = 4 * 1000;
const QLatin1String kIpRegExprValue("[^a-zA-Z0-9\\.](([0-9]){1,3}\\.){3}([0-9]){1,3}[^a-zA-Z0-9\\.]");

}

QnPublicIPDiscovery::QnPublicIPDiscovery(QStringList primaryUrls)
:
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

void QnPublicIPDiscovery::update()
{
    m_stage = Stage::primaryUrlsRequesting;
    for (const QString &url : m_primaryUrls)
        sendRequest(url);
}

void QnPublicIPDiscovery::waitForFinished()
{
    auto waitFunc =
        [this](int timeout)
        {
            QTime t;
            t.start();
            while (t.elapsed() < timeout && m_publicIP.isNull() && m_replyInProgress > 0)
            {
                QnSleep::msleep(1);
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

QHostAddress QnPublicIPDiscovery::publicIP() const
{
    return m_publicIP;
}

void QnPublicIPDiscovery::handleReply(const nx_http::AsyncHttpClientPtr& httpClient)
{
    /* Check if reply finished successfully. */

    if ((httpClient->failed()) ||
        (httpClient->response()->statusLine.statusCode != nx_http::StatusCode::ok))
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

void QnPublicIPDiscovery::sendRequest(const QString &url)
{
    nx_http::AsyncHttpClientPtr httpRequest = nx_http::AsyncHttpClient::create();

    auto at_reply_finished =
        [this, httpRequest](const nx_http::AsyncHttpClientPtr& httpClient) mutable
        {
            handleReply(httpClient);
            httpRequest->disconnect();
            httpRequest.reset();
            m_replyInProgress--;
            if (m_replyInProgress == 0)
                nextStage();

            /* If no public ip found, clean existing. */
            if (m_stage == Stage::idle && !m_publicIP.isNull())
            {
                m_publicIP.clear();
                emit found(m_publicIP);
            }
        };

    NX_LOG(lit("Sending request to %1").arg(url), cl_logDEBUG2);
    m_replyInProgress++;

    httpRequest->setResponseReadTimeoutMs(kRequestTimeoutMs);
    connect(
        httpRequest.get(), &nx_http::AsyncHttpClient::done,
        this, at_reply_finished,
        Qt::DirectConnection);
    httpRequest->doGet(url);
}

void QnPublicIPDiscovery::nextStage()
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

QString QnPublicIPDiscovery::toString(Stage value) const
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
