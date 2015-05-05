#include "direct_module_finder.h"

#include <QtCore/QTimer>

#include <utils/common/model_functions.h>
#include <utils/common/log.h>
#include <utils/network/http/asynchttpclient.h>
#include <utils/network/http/async_http_client_reply.h>
#include <utils/network/networkoptixmodulerevealcommon.h>
#include <rest/server/json_rest_result.h>
#include <common/common_module.h>
#include <nx_ec/ec_proto_version.h>
#include <utils/network/module_information.h>

#include <utils/common/app_info.h>

namespace {

    const int defaultMaxConnections = 30;
    const int discoveryCheckIntervalMs = 60 * 1000;
    const int aliveCheckIntervalMs = 7 * 1000;
    const int timerIntervalMs = 1000;
    const int maxPingTimeoutMs = 15 * 1000;

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
        return result;
    }

} // anonymous namespace

QnDirectModuleFinder::QnDirectModuleFinder(QObject *parent) :
    QObject(parent),
    m_compatibilityMode(false),
    m_maxConnections(defaultMaxConnections),
    m_checkTimer(new QTimer(this))
{
    m_checkTimer->setInterval(timerIntervalMs);
    connect(m_checkTimer, &QTimer::timeout, this, &QnDirectModuleFinder::at_checkTimer_timeout);
}

void QnDirectModuleFinder::setCompatibilityMode(bool compatibilityMode) {
    m_compatibilityMode = compatibilityMode;
}

bool QnDirectModuleFinder::isCompatibilityMode() const {
    return m_compatibilityMode;
}

void QnDirectModuleFinder::addUrl(const QUrl &url) {
    QUrl locUrl = trimmedUrl(url);
    if (!m_urls.contains(locUrl)) {
        m_urls.insert(locUrl);
        enqueRequest(locUrl);
    }
}

void QnDirectModuleFinder::removeUrl(const QUrl &url) {
    QUrl locUrl = trimmedUrl(url);
    m_urls.remove(locUrl);
    m_lastPingByUrl.remove(locUrl);
    m_lastPingByUrl.remove(locUrl);
}

void QnDirectModuleFinder::checkUrl(const QUrl &url) {
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

        nx_http::AsyncHttpClientPtr client = std::make_shared<nx_http::AsyncHttpClient>();
        std::unique_ptr<QnAsyncHttpClientReply> reply( new QnAsyncHttpClientReply(client, this) );
        connect(reply.get(), &QnAsyncHttpClientReply::finished, this, &QnDirectModuleFinder::at_reply_finished);

        if (!client->doGet(url))
            return;

        Q_ASSERT_X(!m_activeRequests.contains(url), "Duplicate request issued", Q_FUNC_INFO);
        m_activeRequests.insert(url, reply.release());
    }
}

void QnDirectModuleFinder::at_reply_finished(QnAsyncHttpClientReply *reply) {
    reply->deleteLater();

    QUrl url = reply->url();
    const auto replyIter = m_activeRequests.find(url);
    Q_ASSERT_X(replyIter != m_activeRequests.end(), "Reply that is not in the set of active requests has finished! (1)", Q_FUNC_INFO);
    Q_ASSERT_X(replyIter.value() == reply, "Reply that is not in the set of active requests has finished! (2)", Q_FUNC_INFO);
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
    QJson::deserialize(result.reply(), &moduleInformation);
    if (moduleInformation.protoVersion == 0)
        moduleInformation.protoVersion = nx_ec::INITIAL_EC2_PROTO_VERSION;

    if (moduleInformation.id.isNull())
        return;

    if (moduleInformation.type != nxMediaServerId)
        return;

    if (!m_compatibilityMode && moduleInformation.customization != QnAppInfo::customizationName())
        return;

    moduleInformation.fixRuntimeId();
    m_lastPingByUrl[url] = m_elapsedTimer.elapsed();

    emit responseReceived(moduleInformation, SocketAddress(url.host(), url.port()));
}

void QnDirectModuleFinder::at_checkTimer_timeout() {
    qint64 currentTime = m_elapsedTimer.elapsed();

    for (const QUrl &url: m_urls) {
        bool alive = currentTime - m_lastPingByUrl.value(url) < maxPingTimeoutMs;
        qint64 lastCheck = m_lastCheckByUrl.value(url);
        if (alive) {
            if (currentTime - lastCheck < aliveCheckIntervalMs)
                continue;
        } else {
            if (currentTime - lastCheck < discoveryCheckIntervalMs)
                continue;
        }
        enqueRequest(url);
    }
    activateRequests();
}
