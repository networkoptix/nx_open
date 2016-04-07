#include "direct_module_finder.h"

#include <QtCore/QTimer>

#include <api/global_settings.h>
#include <common/common_module.h>

#include <network/module_information.h>
#include <network/networkoptixmodulerevealcommon.h>

#include <nx_ec/ec_proto_version.h>
#include <rest/server/json_rest_result.h>

#include <utils/common/app_info.h>
#include <nx/utils/log/log.h>

#include <utils/common/model_functions.h>
#include <nx/network/http/asynchttpclient.h>
#include <nx/network/http/async_http_client_reply.h>

#include <utils/common/app_info.h>


using std::chrono::duration_cast;
using std::chrono::seconds;
using std::chrono::milliseconds;

namespace {

    const int kDefaultMaxConnections = 30;
    const std::chrono::seconds kDiscoveryCheckInterval(60);
    const int kTimerIntervalMs = 1000;
    const int kHttpTimeoutMs = 5000;

    QUrl trimmedUrl(const QUrl &url) {
        QUrl result;
        result.setScheme(lit("http"));
        result.setHost(url.host());
        result.setPort(url.port());
        return result;
    }

    QUrl requestUrl(const QUrl &url) {
        QUrl result = trimmedUrl(url);
        result.setPath(lit("/api/moduleInformation"));
        QUrlQuery query;
        query.addQueryItem(lit("showAddresses"), lit("false"));
        result.setQuery(query);
        return result;
    }

} // anonymous namespace

QnDirectModuleFinder::QnDirectModuleFinder(QObject* parent)
:
    QObject(parent),
    m_compatibilityMode(false),
    m_maxConnections(kDefaultMaxConnections),
    m_checkTimer(new QTimer(this))
{
    m_checkTimer->setInterval(kTimerIntervalMs);
    connect(m_checkTimer, &QTimer::timeout, this, &QnDirectModuleFinder::at_checkTimer_timeout);
}

void QnDirectModuleFinder::setCompatibilityMode(bool compatibilityMode) {
    m_compatibilityMode = compatibilityMode;
}

bool QnDirectModuleFinder::isCompatibilityMode() const {
    return m_compatibilityMode;
}

void QnDirectModuleFinder::addUrl(const QUrl &url) {
    NX_LOG(lit("QnDirectModuleFinder::addUrl %1").arg(url.toString()), cl_logDEBUG2);

    QUrl locUrl = trimmedUrl(url);
    if (!m_urls.contains(locUrl)) {
        m_urls.insert(locUrl);
        enqueRequest(locUrl);
    }
}

void QnDirectModuleFinder::removeUrl(const QUrl &url) {
    NX_LOG(lit("QnDirectModuleFinder::removeUrl %1").arg(url.toString()), cl_logDEBUG2);

    QUrl locUrl = trimmedUrl(url);
    m_urls.remove(locUrl);
    m_lastPingByUrl.remove(locUrl);
    m_lastPingByUrl.remove(locUrl);
}

void QnDirectModuleFinder::checkUrl(const QUrl &url) {
    NX_LOG(lit("QnDirectModuleFinder::checkUrl %1").arg(url.toString()), cl_logDEBUG2);

    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "checkUrl", Q_ARG(QUrl, url));
        return;
    }

    enqueRequest(trimmedUrl(url));
}

void QnDirectModuleFinder::setUrls(const QSet<QUrl> &urls) {
    m_urls.clear();
    for (const QUrl &url: urls)
        m_urls.insert(trimmedUrl(url));
}

QSet<QUrl> QnDirectModuleFinder::urls() const {
    return m_urls;
}

void QnDirectModuleFinder::start() {
    m_elapsedTimer.start();
    at_checkTimer_timeout();
    m_checkTimer->start();
}

void QnDirectModuleFinder::pleaseStop() {
    m_checkTimer->stop();
    m_requestQueue.clear();
    for (QnAsyncHttpClientReply *reply: m_activeRequests) {
        reply->asyncHttpClient()->terminate();
        reply->deleteLater();
    }
    m_activeRequests.clear();
}

void QnDirectModuleFinder::enqueRequest(const QUrl &url) {
    QUrl reqUrl = requestUrl(url);
    if (m_activeRequests.contains(reqUrl) || m_requestQueue.contains(reqUrl))
        return;

    m_requestQueue.enqueue(reqUrl);
    QMetaObject::invokeMethod(this, "activateRequests", Qt::QueuedConnection);
}

void QnDirectModuleFinder::activateRequests() {
    while (m_activeRequests.size() < m_maxConnections && !m_requestQueue.isEmpty()) {
        QUrl url = m_requestQueue.dequeue();

        qint64 time = m_elapsedTimer.elapsed();
        m_lastCheckByUrl[trimmedUrl(url)] = time;

        nx_http::AsyncHttpClientPtr client = nx_http::AsyncHttpClient::create();
        client->setSendTimeoutMs(kHttpTimeoutMs);
        std::unique_ptr<QnAsyncHttpClientReply> reply( new QnAsyncHttpClientReply(client, this) );
        connect(reply.get(), &QnAsyncHttpClientReply::finished, this, &QnDirectModuleFinder::at_reply_finished);

        client->doGet(url);

        NX_ASSERT(!m_activeRequests.contains(url), "Duplicate request issued", Q_FUNC_INFO);
        m_activeRequests.insert(url, reply.release());
    }

    if (!m_requestQueue.isEmpty())
    {
        NX_LOG(lit("QnDirectModuleFinder::activateRequests. Currently, we have %1 requests to:")
            .arg(m_activeRequests.size()), cl_logDEBUG2);
        for (const auto& requestData: m_activeRequests)
        {
            NX_LOG(lit("        %1").arg(requestData->url().toString()), cl_logDEBUG2);
        }
        NX_LOG(lit("Pending requests:"), cl_logDEBUG2);
        for (const auto& moduleUrl: m_requestQueue)
        {
            NX_LOG(lit("        %1").arg(moduleUrl.toString()), cl_logDEBUG2);
        }
    }
}

void QnDirectModuleFinder::at_reply_finished(QnAsyncHttpClientReply *reply) {
    reply->deleteLater();

    QUrl url = reply->url();

    NX_LOG(lit("QnDirectModuleFinder. Received %1 reply from %2")
        .arg(!reply->isFailed()).arg(url.toString()), cl_logDEBUG1);

    const auto replyIter = m_activeRequests.find(url);
    NX_ASSERT(replyIter != m_activeRequests.end(), "Reply that is not in the set of active requests has finished! (1)", Q_FUNC_INFO);
    NX_ASSERT(replyIter.value() == reply, "Reply that is not in the set of active requests has finished! (2)", Q_FUNC_INFO);
    if (replyIter != m_activeRequests.end())
        m_activeRequests.erase(replyIter);

    activateRequests();

    if (reply->isFailed())
        return;

    url = trimmedUrl(url);

    QByteArray data = reply->data();

    QnJsonRestResult result;
    QJson::deserialize(data, &result);
    QnModuleInformation moduleInformation;
    QJson::deserialize(result.reply, &moduleInformation);
    if (moduleInformation.protoVersion == 0)
        moduleInformation.protoVersion = nx_ec::INITIAL_EC2_PROTO_VERSION;

    if (moduleInformation.id.isNull())
    {
        NX_LOG(lit("QnDirectModuleFinder. Received empty module id from %1. Ignoring reply...")
            .arg(url.toString()), cl_logDEBUG1);
        return;
    }

    if (moduleInformation.type != QnModuleInformation::nxMediaServerId())
    {
        NX_LOG(lit("QnDirectModuleFinder. Received reply with improper module type (%1) id from %2. Ignoring reply...")
            .arg(moduleInformation.type).arg(url.toString()), cl_logDEBUG1);
        return;
    }

    if (!m_compatibilityMode && moduleInformation.customization != QnAppInfo::customizationName())
    {
        NX_LOG(lit("QnDirectModuleFinder. Received reply from imcompatible server: url %1, customization %2. Ignoring reply...")
            .arg(url.toString()).arg(moduleInformation.customization), cl_logDEBUG1);
        return;
    }

    moduleInformation.fixRuntimeId();
    m_lastPingByUrl[url] = m_elapsedTimer.elapsed();

    NX_LOG(lit("QnDirectModuleFinder. Received success reply from url %1")
        .arg(url.toString()), cl_logDEBUG1);

    emit responseReceived(moduleInformation, SocketAddress(url.host(), url.port()));
}

void QnDirectModuleFinder::at_checkTimer_timeout() {
    NX_LOG(lit("QnDirectModuleFinder::at_checkTimer_timeout"), cl_logDEBUG2);

    qint64 currentTime = m_elapsedTimer.elapsed();

    for (const QUrl &url: m_urls) {
        NX_LOG(lit("QnDirectModuleFinder::at_checkTimer_timeout. url %1").arg(url.toString()), cl_logDEBUG2);

        const bool alive = currentTime - m_lastPingByUrl.value(url) < maxPingTimeout().count();
        const qint64 lastCheck = m_lastCheckByUrl.value(url);
        if (alive) {
            if (currentTime - lastCheck < aliveCheckInterval().count())
            {
                NX_LOG(lit("QnDirectModuleFinder::at_checkTimer_timeout. url %1. Not adding (1) since %2 < %3")
                    .arg(url.toString()).arg(currentTime - lastCheck).arg(aliveCheckInterval().count()), cl_logDEBUG2);
                continue;
            }
        } else {
            if (currentTime - lastCheck < discoveryCheckInterval().count())
            {
                NX_LOG(lit("QnDirectModuleFinder::at_checkTimer_timeout. url %1. Not adding (2) since %2 < %3")
                    .arg(url.toString()).arg(currentTime - lastCheck).arg(discoveryCheckInterval().count()), cl_logDEBUG2);
                continue;
            }
        }
        enqueRequest(url);
    }
    activateRequests();
}

std::chrono::milliseconds QnDirectModuleFinder::maxPingTimeout() const
{
    return QnGlobalSettings::instance()->serverDiscoveryAliveCheckTimeout();
}

std::chrono::milliseconds QnDirectModuleFinder::aliveCheckInterval() const
{
    const auto maxPingTimeoutLocal = maxPingTimeout();
    return maxPingTimeoutLocal > seconds(4)
        ? (maxPingTimeoutLocal / 2) - seconds(1)
        : seconds(1);
}

std::chrono::milliseconds QnDirectModuleFinder::discoveryCheckInterval() const
{
    return QnGlobalSettings::instance()->serverDiscoveryPingTimeout();
}
