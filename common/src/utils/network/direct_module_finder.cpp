#include "direct_module_finder.h"

#include <QtCore/QTimer>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkProxy>

#include <utils/common/model_functions.h>
#include <utils/common/log.h>
#include <utils/network/http/asynchttpclient.h>
#include <utils/network/http/async_http_client_reply.h>
#include <utils/network/networkoptixmodulerevealcommon.h>
#include <rest/server/json_rest_result.h>
#include <common/common_module.h>

#include <utils/common/app_info.h>

namespace {

    const int defaultMaxConnections = 30;
    const int discoveryCheckIntervalMs = 60 * 1000;
    const int aliveCheckIntervalMs = 7 * 1000;
    const int maxPingTimeoutMs = 12 * 1000;

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
    m_maxConnections(defaultMaxConnections),
    m_compatibilityMode(false),
    m_discoveryCheckTimer(new QTimer(this)),
    m_aliveCheckTimer(new QTimer(this))
{
    m_discoveryCheckTimer->setInterval(discoveryCheckIntervalMs);
    m_aliveCheckTimer->setInterval(aliveCheckIntervalMs);
    connect(m_discoveryCheckTimer,  &QTimer::timeout,                   this,   &QnDirectModuleFinder::at_discoveryCheckTimer_timeout);
    connect(m_aliveCheckTimer,      &QTimer::timeout,                   this,   &QnDirectModuleFinder::at_aliveCheckTimer_timeout);
}

void QnDirectModuleFinder::setCompatibilityMode(bool compatibilityMode) {
    m_compatibilityMode = compatibilityMode;
}

bool QnDirectModuleFinder::isCompatibilityMode() const
{
    return m_compatibilityMode;
}

void QnDirectModuleFinder::addUrl(const QUrl &url, const QnUuid &id) {
    Q_ASSERT(id != qnCommon->moduleGUID());

    QUrl locUrl = trimmedUrl(url);
    if (!m_urls.contains(locUrl, id)) {
        m_urls.insert(locUrl, id);
        enqueRequest(locUrl);
    }
}

void QnDirectModuleFinder::removeUrl(const QUrl &url, const QnUuid &id) {
    QUrl locUrl = trimmedUrl(url);
    if (m_urls.remove(locUrl, id)) {
        if (m_lastPingByUrl.take(locUrl) != 0) {
            QnUuid id = m_moduleByUrl.take(locUrl);
            NX_LOG(lit("QnDirectModuleFinder: Url %2 of the module %1 is removed.").arg(id.toString()).arg(locUrl.toString()), cl_logDEBUG1);
            emit moduleUrlLost(m_foundModules.value(id), locUrl);
        }
    }
}

void QnDirectModuleFinder::addIgnoredModule(const QUrl &url, const QnUuid &id) {
    QUrl locUrl = trimmedUrl(url);
    if (!m_ignoredModules.contains(locUrl, id)) {
        m_ignoredModules.insert(locUrl, id);

        if (m_moduleByUrl.value(locUrl) == id) {
            m_lastPingByUrl.remove(locUrl);
            m_moduleByUrl.remove(locUrl);
            NX_LOG(lit("QnDirectModuleFinder: Url %2 of the module %1 is removed.").arg(id.toString()).arg(locUrl.toString()), cl_logDEBUG1);
            emit moduleUrlLost(m_foundModules.value(id), locUrl);
        }
    }
}

void QnDirectModuleFinder::removeIgnoredModule(const QUrl &url, const QnUuid &id) {
    m_ignoredModules.remove(trimmedUrl(url), id);
}

void QnDirectModuleFinder::addIgnoredUrl(const QUrl &url) {
    QUrl locUrl = trimmedUrl(url);
    m_ignoredUrls.insert(locUrl);
    if (m_lastPingByUrl.take(locUrl) != 0) {
        QnUuid id = m_moduleByUrl.take(locUrl);
        NX_LOG(lit("QnDirectModuleFinder: Url %2 of the module %1 is removed.").arg(id.toString()).arg(locUrl.toString()), cl_logDEBUG1);
        emit moduleUrlLost(m_foundModules.value(id), locUrl);
    }
}

void QnDirectModuleFinder::removeIgnoredUrl(const QUrl &url) {
    m_ignoredUrls.remove(trimmedUrl(url));
}

void QnDirectModuleFinder::checkUrl(const QUrl &url) {
    enqueRequest(url);
}

void QnDirectModuleFinder::start() {
    at_discoveryCheckTimer_timeout();
    m_discoveryCheckTimer->start();
    m_aliveCheckTimer->start();
}

void QnDirectModuleFinder::stop() {
    m_discoveryCheckTimer->stop();
    m_aliveCheckTimer->stop();
    m_requestQueue.clear();
    m_activeRequests.clear();
}

void QnDirectModuleFinder::pleaseStop() {
    m_discoveryCheckTimer->stop();
    m_requestQueue.clear();
    m_activeRequests.clear();
}

QList<QnModuleInformation> QnDirectModuleFinder::foundModules() const {
    return m_foundModules.values();
}

QnModuleInformation QnDirectModuleFinder::moduleInformation(const QnUuid &id) const {
    return m_foundModules[id];
}

QMultiHash<QUrl, QnUuid> QnDirectModuleFinder::urls() const {
    return m_urls;
}

QMultiHash<QUrl, QnUuid> QnDirectModuleFinder::ignoredModules() const {
    return m_ignoredModules;
}

QSet<QUrl> QnDirectModuleFinder::ignoredUrls() const {
    return m_ignoredUrls;
}

void QnDirectModuleFinder::enqueRequest(const QUrl &url) {
    QUrl reqUrl = requestUrl(url);
    if (m_activeRequests.contains(reqUrl) || m_requestQueue.contains(reqUrl))
        return;

    m_requestQueue.enqueue(reqUrl);
    QTimer::singleShot(0, this, SLOT(activateRequests()));
}

void QnDirectModuleFinder::activateRequests() {
    while (m_activeRequests.size() < m_maxConnections && !m_requestQueue.isEmpty()) {
        QUrl url = m_requestQueue.dequeue();

        nx_http::AsyncHttpClientPtr client = std::make_shared<nx_http::AsyncHttpClient>();
        QnAsyncHttpClientReply *reply = new QnAsyncHttpClientReply(client, this);
        connect(reply, &QnAsyncHttpClientReply::finished, this, &QnDirectModuleFinder::at_reply_finished);

        m_activeRequests.insert(url);
        client->doGet(url);
    }
}

void QnDirectModuleFinder::at_reply_finished(QnAsyncHttpClientReply *reply) {
    reply->deleteLater();

    QUrl url = reply->url();
    if (!m_activeRequests.remove(url))
        Q_ASSERT_X(0, "Reply that is not in the set of active requests has finished!", Q_FUNC_INFO);

    activateRequests();

    url = trimmedUrl(url);

    if (!reply->isFailed()) {
        QByteArray data = reply->data();

        QnJsonRestResult result;
        QJson::deserialize(data, &result);
        QnModuleInformation moduleInformation;
        QJson::deserialize(result.reply(), &moduleInformation);

        if (moduleInformation.id.isNull())
            return;

        if (moduleInformation.type != nxMediaServerId)
            return;

        if (!m_compatibilityMode && moduleInformation.customization != QnAppInfo::customizationName())
            return;

        if (m_ignoredModules.contains(url, moduleInformation.id))
            return;

        if (m_ignoredUrls.contains(url))
            return;

        QSet<QnUuid> expectedIds = QSet<QnUuid>::fromList(m_urls.values(url));
        if (!expectedIds.isEmpty() && !expectedIds.contains(moduleInformation.id))
            return;

        QnModuleInformation &oldModuleInformation = m_foundModules[moduleInformation.id];
        qint64 &lastPing = m_lastPingByUrl[url];
        QnUuid &moduleId = m_moduleByUrl[url];

        moduleInformation.remoteAddresses.clear();
        moduleInformation.remoteAddresses.insert(url.host());
        moduleInformation.remoteAddresses.unite(oldModuleInformation.remoteAddresses);

        if (!moduleId.isNull() && moduleId != moduleInformation.id) {
            QnModuleInformation &prevModuleInformation = m_foundModules[moduleId];
            prevModuleInformation.remoteAddresses.remove(url.host());
            NX_LOG(lit("QnDirectModuleFinder: Url %2 of the module %1 is lost.").arg(prevModuleInformation.id.toString()).arg(url.toString()), cl_logDEBUG1);
            emit moduleChanged(prevModuleInformation);
            emit moduleUrlLost(prevModuleInformation, url);
            lastPing = 0;
        }
        moduleId = moduleInformation.id;

        if (oldModuleInformation != moduleInformation) {
            oldModuleInformation = moduleInformation;
            emit moduleChanged(moduleInformation);
        }

        if (lastPing == 0) {
            NX_LOG(lit("QnDirectModuleFinder: Url %2 of the module %1 is found.").arg(moduleInformation.id.toString()).arg(url.toString()), cl_logDEBUG1);
            emit moduleUrlFound(moduleInformation, url);
        }

        lastPing = QDateTime::currentMSecsSinceEpoch();
    } else {
        QnUuid id = m_moduleByUrl.value(url);
        if (id.isNull())
            return;

        if (m_lastPingByUrl.value(url) + maxPingTimeoutMs < QDateTime::currentMSecsSinceEpoch()) {
            m_moduleByUrl.remove(url);
            m_lastPingByUrl.remove(url);
            QnModuleInformation &moduleInformation = m_foundModules[id];
            moduleInformation.remoteAddresses.remove(url.host());
            NX_LOG(lit("QnDirectModuleFinder: Url %2 of the module %1 is lost.").arg(moduleInformation.id.toString()).arg(url.toString()), cl_logDEBUG1);
            emit moduleChanged(moduleInformation);
            emit moduleUrlLost(moduleInformation, url);
        }
    }
}

void QnDirectModuleFinder::at_discoveryCheckTimer_timeout() {
    QSet<QUrl> urls = QSet<QUrl>::fromList(m_urls.keys()) - m_ignoredUrls - QSet<QUrl>::fromList(m_lastPingByUrl.keys());

    for (const QUrl &url: urls)
        enqueRequest(url);
}

void QnDirectModuleFinder::at_aliveCheckTimer_timeout() {
    for (const QUrl &url: m_lastPingByUrl.keys())
        enqueRequest(url);
}
