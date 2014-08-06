#include "direct_module_finder.h"

#include <QtCore/QTimer>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

#include <utils/common/model_functions.h>
#include <rest/server/json_rest_result.h>
#include <common/common_module.h>

#include <version.h>

namespace {

    const int defaultMaxConnections = 30;
    const int periodicalCheckIntervalMs = 15 * 1000;
    const int maxPingTimeoutMs = 60 * 1000;

    QUrl makeRequestUrl(const QHostAddress &address, quint16 port) {
        return QUrl(QString(lit("http://%1:%2/api/moduleInformation")).arg(address.toString()).arg(port));
    }

    QUrl makeRequestUrl(const QUrl &url) {
        QUrl result;
        result.setScheme(url.scheme());
        result.setHost(url.host());
        result.setPort(url.port());
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
    connect(m_periodicalCheckTimer, &QTimer::timeout,                   this,   &QnDirectModuleFinder::at_periodicalCheckTimer_timeout);
    connect(m_networkAccessManager, &QNetworkAccessManager::finished,   this,   &QnDirectModuleFinder::at_reply_finished);
}

void QnDirectModuleFinder::setCompatibilityMode(bool compatibilityMode) {
    m_compatibilityMode = compatibilityMode;
}

void QnDirectModuleFinder::dropModule(const QnId &id, bool emitSignal) {
    QnModuleInformation moduleInformation = m_foundModules.take(id);
    if (moduleInformation.id.isNull())
        return;

    foreach (const QUrl &url, m_moduleByUrl.keys(id))
        m_moduleByUrl.remove(url);
    m_lastPingById.remove(id);

    if (emitSignal)
        emit moduleLost(moduleInformation);
}

void QnDirectModuleFinder::dropModule(const QUrl &url, bool emitSignal) {
    QnId id = m_moduleByUrl.take(url);
    if (id.isNull())
        return;

    if (m_moduleByUrl.keys(id).isEmpty())
        dropModule(id, emitSignal);
}

void QnDirectModuleFinder::addUrl(const QUrl &url, const QnId &id) {
    Q_ASSERT(id != qnCommon->moduleGUID());

    QUrl requestUrl = makeRequestUrl(url);
    if (!m_urls.contains(requestUrl, id)) {
        m_urls.insert(requestUrl, id);
        enqueRequest(requestUrl);
    }
}

void QnDirectModuleFinder::removeUrl(const QUrl &url, const QnId &id) {
    if (m_urls.remove(makeRequestUrl(url), id))
        dropModule(id, false);
}

void QnDirectModuleFinder::addAddress(const QHostAddress &address, quint16 port, const QnId &id) {
    addUrl(makeRequestUrl(address, port), id);
}

void QnDirectModuleFinder::removeAddress(const QHostAddress &address, quint16 port, const QnId &id) {
    removeUrl(makeRequestUrl(address, port), id);
}

void QnDirectModuleFinder::addIgnoredModule(const QUrl &url, const QnId &id) {
    QUrl requestUrl = makeRequestUrl(url);
    if (!m_ignoredModules.contains(requestUrl, id)) {
        m_ignoredModules.insert(requestUrl, id);
        enqueRequest(requestUrl);

        dropModule(id, false);
    }
}

void QnDirectModuleFinder::removeIgnoredModule(const QUrl &url, const QnId &id) {
    m_ignoredModules.remove(makeRequestUrl(url), id);
}

void QnDirectModuleFinder::addIgnoredModule(const QHostAddress &address, quint16 port, const QnId &id) {
    addIgnoredModule(makeRequestUrl(address, port), id);
}

void QnDirectModuleFinder::removeIgnoredModule(const QHostAddress &address, quint16 port, const QnId &id) {
    removeIgnoredModule(makeRequestUrl(address, port), id);
}

void QnDirectModuleFinder::addIgnoredUrl(const QUrl &url) {
    QUrl requestUrl = makeRequestUrl(url);
    m_ignoredUrls.insert(requestUrl);
    dropModule(requestUrl);
}

void QnDirectModuleFinder::removeIgnoredUrl(const QUrl &url) {
    m_ignoredUrls.remove(makeRequestUrl(url));
}

void QnDirectModuleFinder::addIgnoredAddress(const QHostAddress &address, quint16 port) {
    addIgnoredUrl(makeRequestUrl(address, port));
}

void QnDirectModuleFinder::removeIgnoredAddress(const QHostAddress &address, quint16 port) {
    m_ignoredUrls.remove(makeRequestUrl(address, port));
}

void QnDirectModuleFinder::checkUrl(const QUrl &url) {
    QUrl fixedUrl = url;
    fixedUrl.setPath(lit("/api/moduleInformation"));
    enqueRequest(fixedUrl);
}

void QnDirectModuleFinder::start() {
    at_periodicalCheckTimer_timeout();
    m_periodicalCheckTimer->start();
}

void QnDirectModuleFinder::stop() {
    m_periodicalCheckTimer->stop();
    m_requestQueue.clear();
}

QList<QnModuleInformation> QnDirectModuleFinder::foundModules() const {
    return m_foundModules.values();
}

QnModuleInformation QnDirectModuleFinder::moduleInformation(const QnId &id) const {
    return m_foundModules[id];
}

QMultiHash<QUrl, QnId> QnDirectModuleFinder::urls() const {
    return m_urls;
}

QMultiHash<QUrl, QnId> QnDirectModuleFinder::ignoredModules() const {
    return m_ignoredModules;
}

QSet<QUrl> QnDirectModuleFinder::ignoredUrls() const {
    return m_ignoredUrls;
}

void QnDirectModuleFinder::enqueRequest(const QUrl &url) {
    if (m_activeRequests.contains(url) || m_requestQueue.contains(url))
        return;

    m_requestQueue.enqueue(url);
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

        QnIdSet expectedIds = QnIdSet::fromList(m_urls.values(url));
        if (!expectedIds.isEmpty() && !expectedIds.contains(moduleInformation.id))
            return;

        // we don't want use addresses reported by the server
        moduleInformation.remoteAddresses.clear();
        // TODO: #dklychkov deal with dns names
        moduleInformation.remoteAddresses.insert(url.host());

        auto it = m_foundModules.find(moduleInformation.id);
        if (it == m_foundModules.end()) {
            m_foundModules.insert(moduleInformation.id, moduleInformation);
        } else {
            moduleInformation.remoteAddresses += it->remoteAddresses;
            *it = moduleInformation;
        }

        m_lastPingById[moduleInformation.id] = QDateTime::currentMSecsSinceEpoch();

        if (!m_urls.contains(url, moduleInformation.id))
            m_urls.insert(url, moduleInformation.id);

        url.setPath(QString());
        emit moduleFound(moduleInformation, url.host(), url);
    } else {
        QnId id = m_moduleByUrl[url];
        if (id.isNull())
            return;

        if (m_lastPingById[id] + maxPingTimeoutMs < QDateTime::currentMSecsSinceEpoch())
            dropModule(id, true);
    }
}

void QnDirectModuleFinder::at_periodicalCheckTimer_timeout() {
    QSet<QUrl> urls = QSet<QUrl>::fromList(m_urls.keys()) - m_ignoredUrls;

    foreach (const QUrl &url, urls)
        enqueRequest(url);
}
