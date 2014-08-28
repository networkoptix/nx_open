#include "direct_module_finder.h"

#include <QtCore/QTimer>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkProxy>

#include <utils/common/model_functions.h>
#include <rest/server/json_rest_result.h>
#include <common/common_module.h>

#include <version.h>

namespace {

    const int defaultMaxConnections = 30;
    const int periodicalCheckIntervalMs = 15 * 1000;
    const int maxPingTimeoutMs = 60 * 1000;

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
    m_networkAccessManager(new QNetworkAccessManager(this)),
    m_periodicalCheckTimer(new QTimer(this))
{
    m_periodicalCheckTimer->setInterval(periodicalCheckIntervalMs);
    m_networkAccessManager->setProxy(QNetworkProxy(QNetworkProxy::NoProxy));

    connect(m_periodicalCheckTimer, &QTimer::timeout,                   this,   &QnDirectModuleFinder::at_periodicalCheckTimer_timeout);
    connect(m_networkAccessManager, &QNetworkAccessManager::finished,   this,   &QnDirectModuleFinder::at_reply_finished);
}

void QnDirectModuleFinder::setCompatibilityMode(bool compatibilityMode) {
    m_compatibilityMode = compatibilityMode;
}

void QnDirectModuleFinder::addUrl(const QUrl &url, const QUuid &id) {
    Q_ASSERT(id != qnCommon->moduleGUID());

    QUrl locUrl = trimmedUrl(url);
    if (!m_urls.contains(locUrl, id)) {
        m_urls.insert(locUrl, id);
        enqueRequest(locUrl);
    }
}

void QnDirectModuleFinder::removeUrl(const QUrl &url, const QUuid &id) {
    QUrl locUrl = trimmedUrl(url);
    if (m_urls.remove(locUrl, id)) {
        if (m_lastPingByUrl.take(locUrl) != 0) {
            QUuid id = m_moduleByUrl.take(locUrl);
            emit moduleUrlLost(m_foundModules.value(id), locUrl);
        }
    }
}

void QnDirectModuleFinder::addIgnoredModule(const QUrl &url, const QUuid &id) {
    QUrl locUrl = trimmedUrl(url);
    if (!m_ignoredModules.contains(locUrl, id)) {
        m_ignoredModules.insert(locUrl, id);

        if (m_moduleByUrl.value(locUrl) == id) {
            m_lastPingByUrl.remove(locUrl);
            m_moduleByUrl.remove(locUrl);
            emit moduleUrlLost(m_foundModules.value(id), locUrl);
        }
    }
}

void QnDirectModuleFinder::removeIgnoredModule(const QUrl &url, const QUuid &id) {
    m_ignoredModules.remove(trimmedUrl(url), id);
}

void QnDirectModuleFinder::addIgnoredUrl(const QUrl &url) {
    QUrl locUrl = trimmedUrl(url);
    m_ignoredUrls.insert(locUrl);
    if (m_lastPingByUrl.take(locUrl) != 0) {
        QUuid id = m_moduleByUrl.take(locUrl);
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
    at_periodicalCheckTimer_timeout();
    m_periodicalCheckTimer->start();
}

void QnDirectModuleFinder::stop() {
    m_periodicalCheckTimer->stop();
    m_requestQueue.clear();
}

void QnDirectModuleFinder::pleaseStop() {
    m_periodicalCheckTimer->stop();
    m_requestQueue.clear();
}

QList<QnModuleInformation> QnDirectModuleFinder::foundModules() const {
    return m_foundModules.values();
}

QnModuleInformation QnDirectModuleFinder::moduleInformation(const QUuid &id) const {
    return m_foundModules[id];
}

QMultiHash<QUrl, QUuid> QnDirectModuleFinder::urls() const {
    return m_urls;
}

QMultiHash<QUrl, QUuid> QnDirectModuleFinder::ignoredModules() const {
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
        m_activeRequests.insert(url);
        m_networkAccessManager->get(QNetworkRequest(url));
    }
}

void QnDirectModuleFinder::at_reply_finished(QNetworkReply *reply) {
    QUrl url = reply->request().url();
    if (!m_activeRequests.contains(url))
        Q_ASSERT_X(0, "Reply that is not in the set of active requests has finished!", Q_FUNC_INFO);

    reply->deleteLater();

    m_activeRequests.remove(url);
    activateRequests();

    url = trimmedUrl(url);

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();

        QnJsonRestResult result;
        QJson::deserialize(data, &result);
        QnModuleInformation moduleInformation;
        QJson::deserialize(result.reply(), &moduleInformation);

        if (moduleInformation.id.isNull())
            return;

        if (!m_compatibilityMode && moduleInformation.customization != lit(QN_CUSTOMIZATION_NAME))
            return;

        if (m_ignoredModules.contains(url, moduleInformation.id))
            return;

        if (m_ignoredUrls.contains(url))
            return;

        QSet<QUuid> expectedIds = QSet<QUuid>::fromList(m_urls.values(url));
        if (!expectedIds.isEmpty() && !expectedIds.contains(moduleInformation.id))
            return;

        QnModuleInformation &oldModuleInformation = m_foundModules[moduleInformation.id];
        qint64 &lastPing = m_lastPingByUrl[url];
        QUuid &moduleId = m_moduleByUrl[url];

        if (oldModuleInformation != moduleInformation) {
            oldModuleInformation = moduleInformation;
            emit moduleChanged(moduleInformation);
        }

        if (!moduleId.isNull() && moduleId != moduleInformation.id) {
            emit moduleUrlLost(m_foundModules[moduleId], url);
            lastPing = 0;
        }
        moduleId = moduleInformation.id;

        if (lastPing == 0)
            emit moduleUrlFound(moduleInformation, url);

        lastPing = QDateTime::currentMSecsSinceEpoch();
    } else {
        QUuid id = m_moduleByUrl.value(url);
        if (id.isNull())
            return;

        if (m_lastPingByUrl.value(url) + maxPingTimeoutMs < QDateTime::currentMSecsSinceEpoch()) {
            m_moduleByUrl.remove(url);
            m_lastPingByUrl.remove(url);
            emit moduleUrlLost(m_foundModules.value(id), url);
        }
    }
}

void QnDirectModuleFinder::at_periodicalCheckTimer_timeout() {
    QSet<QUrl> urls = QSet<QUrl>::fromList(m_urls.keys()) - m_ignoredUrls;

    foreach (const QUrl &url, urls)
        enqueRequest(url);
}
