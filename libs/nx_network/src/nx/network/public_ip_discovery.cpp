// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "public_ip_discovery.h"

#include <chrono>
#include <regex>

#include <QtCore/QCoreApplication>
#include <QtCore/QElapsedTimer>

#include <nx/branding.h>
#include <nx/network/http/http_client.h>
#include <nx/utils/log/format.h>
#include <nx/utils/log/log.h>

namespace {

const QString kDefaultPrimaryUrlsList(QLatin1String("https://tools.vmsproxy.com/myip"));
const QString kDefaultSecondaryUrlsList(NX_FMT(
    "https://tools-eu.vmsproxy.com/myip;https://%1/myip", nx::branding::cloudHost()));

const int kRequestTimeoutMs = 10 * 1000;
const std::string kIpRegExprValue("[^a-zA-Z0-9\\.](([0-9]){1,3}\\.){3}([0-9]){1,3}[^a-zA-Z0-9\\.]");
static const std::regex kIpRegExpr(kIpRegExprValue);

} // namespace

namespace nx {
namespace network {

PublicIPDiscovery::PublicIPDiscovery(QStringList primaryUrls):
    m_stage(Stage::idle),
    m_primaryUrls(std::move(primaryUrls))
{
    if (m_primaryUrls.isEmpty())
        m_primaryUrls = kDefaultPrimaryUrlsList.split(";", Qt::SkipEmptyParts);
    m_secondaryUrls = kDefaultSecondaryUrlsList.split(";", Qt::SkipEmptyParts);

    if (m_primaryUrls.isEmpty())
    {
        m_primaryUrls = m_secondaryUrls;
        m_secondaryUrls.clear();
    }

    NX_VERBOSE(this, nx::format("Primary urls: %1").arg(m_primaryUrls.join("; ")));
    NX_VERBOSE(this, nx::format("Secondary urls: %1").arg(m_secondaryUrls.join("; ")));

    NX_ASSERT(
        !m_primaryUrls.isEmpty(),
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

void PublicIPDiscovery::setStage(PublicIPDiscovery::Stage value)
{
    m_stage = value;
    NX_VERBOSE(this, nx::format("Set stage to %1").arg(toString(m_stage)));
}

void PublicIPDiscovery::update()
{
    if (m_stage == Stage::primaryUrlsRequesting || m_stage == Stage::secondaryUrlsRequesting)
    {
        NX_VERBOSE(this, "Skip new update requests because current request is in progress.");
        return;
    }

    setStage(Stage::primaryUrlsRequesting);

    NX_MUTEX_LOCKER lock(&m_mutex);
    for (const QString &url : m_primaryUrls)
        sendRequestUnsafe(url);
}

int PublicIPDiscovery::requestsInProgress() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_httpRequests.size();
}

void PublicIPDiscovery::waitForFinished()
{
    auto waitFunc =
        [this](int timeout)
        {
            QElapsedTimer t;
            t.start();
            while (t.elapsed() < timeout && m_publicIP.isNull() && requestsInProgress() > 0)
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
        NX_VERBOSE(this, "Giving additional timeout");
        waitFunc(kRequestTimeoutMs);
    }
}

QHostAddress PublicIPDiscovery::publicIP() const
{
    return m_publicIP;
}

void PublicIPDiscovery::setPublicIp(const QHostAddress& ip)
{
    m_publicIP = ip;
}

void PublicIPDiscovery::handleReply(const nx::network::http::AsyncHttpClientPtr& httpClient)
{
    /* Check if reply finished successfully. */

    if ((httpClient->failed()) ||
        (httpClient->response()->statusLine.statusCode != nx::network::http::StatusCode::ok))
    {
        return;
    }

    /* Check if reply contents contain any ip address. */
    const std::string response =
        std::string(" ") + httpClient->fetchMessageBodyBuffer().toStdString() + " ";

    std::smatch match;
    if (!std::regex_search(response, match, kIpRegExpr) || match.size() < 1)
        return;

    std::string result = match[0].str();
    result = result.substr(1, result.size() - 2);
    if (result.empty())
        return;

    QHostAddress newAddress(QString::fromStdString(result));
    if (!newAddress.isNull())
    {
        NX_VERBOSE(this, nx::format("Found public IP address %1").arg(newAddress.toString()));
        setStage(Stage::publicIpFound);
        if (newAddress != m_publicIP)
        {
            m_publicIP = newAddress;
            emit found(m_publicIP);
        }
    }
}

void PublicIPDiscovery::sendRequestUnsafe(const QString &url)
{
    nx::network::http::AsyncHttpClientPtr httpRequest =
        nx::network::http::AsyncHttpClient::create(nx::network::ssl::kDefaultCertificateCheck);
    httpRequest->bindToAioThread(getAioThread());
    m_httpRequests.insert(httpRequest);

    auto at_reply_finished =
        [this](const nx::network::http::AsyncHttpClientPtr& httpClient) mutable
        {
            handleReply(httpClient);
            httpClient->disconnect();

            {
                NX_MUTEX_LOCKER lock(&m_mutex);
                m_httpRequests.erase(httpClient);
            }
            if (requestsInProgress() == 0)
                nextStage();

            /* If no public ip found, clean existing. */
            if (m_stage == Stage::idle && !m_publicIP.isNull())
            {
                NX_VERBOSE(this, "Public IP address is not found");
                m_publicIP.clear();
                emit found(m_publicIP);
            }
        };

    NX_VERBOSE(this, "Sending request to %1", url);

    httpRequest->setResponseReadTimeoutMs(kRequestTimeoutMs);
    httpRequest->doGet(url, at_reply_finished);
}

void PublicIPDiscovery::nextStage()
{
    if (m_stage == Stage::publicIpFound)
        return;

    if (m_stage == Stage::primaryUrlsRequesting && !m_secondaryUrls.isEmpty())
    {
        setStage(Stage::secondaryUrlsRequesting);

        NX_MUTEX_LOCKER lock(&m_mutex);
        for (const QString &url : m_secondaryUrls)
            sendRequestUnsafe(url);
    }
    else
    {
        setStage(Stage::idle);
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
            return "idle";
        case Stage::primaryUrlsRequesting:
            return "primaryUrlsRequesting";
        case Stage::secondaryUrlsRequesting:
            return "secondaryUrlsRequesting";
        case Stage::publicIpFound:
            return "publicIpFound";
        default:
            NX_ASSERT(false);
            return QString();
    }
}

} // namespace network
} // namespace nx
