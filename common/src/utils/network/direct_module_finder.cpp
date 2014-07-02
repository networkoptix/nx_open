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
    const int manualCheckIntervalMs = 15 * 1000;
    const int maxPingTimeoutMs = 3 * 60 * 1000;

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
    m_manualCheckTimer(new QTimer(this))
{
    m_manualCheckTimer->setInterval(manualCheckIntervalMs);
    connect(m_manualCheckTimer,     &QTimer::timeout,                   this,   &QnDirectModuleFinder::at_manualCheckTimer_timeout);
    connect(m_networkAccessManager, &QNetworkAccessManager::finished,   this,   &QnDirectModuleFinder::at_reply_finished);
}

void QnDirectModuleFinder::setCompatibilityMode(bool compatibilityMode) {
    m_compatibilityMode = compatibilityMode;
}

QnIdSet QnDirectModuleFinder::expectedModuleIds(const QUrl &url) const {
    QnIdSet ids = QnIdSet::fromList(m_manualUrls.values(url));
    ids += QnIdSet::fromList(m_autoUrls.values(url));
    return ids;
}

void QnDirectModuleFinder::checkAndAddUrl(const QUrl &srcUrl, const QnId &id, QMultiHash<QUrl, QnId> *urls) {
    QUrl url = makeRequestUrl(srcUrl);
    if (!urls->contains(url, id)) {
        urls->insert(url, id);
        enqueRequest(url);
    }
}

void QnDirectModuleFinder::checkAndAddAddress(const QHostAddress &address, quint16 port, const QnId &id, QMultiHash<QUrl, QnId> *urls) {
    QUrl url = makeRequestUrl(address, port);
    if (!urls->contains(url, id)) {
        urls->insert(url, id);
        enqueRequest(url);
    }
}

void QnDirectModuleFinder::addUrl(const QUrl &url, const QnId &id) {
    Q_ASSERT(id != qnCommon->moduleGUID());
    checkAndAddUrl(url, id, &m_autoUrls);
}

void QnDirectModuleFinder::removeUrl(const QUrl &url, const QnId &id) {
    m_autoUrls.remove(makeRequestUrl(url), id);
}

void QnDirectModuleFinder::addAddress(const QHostAddress &address, quint16 port, const QnId &id) {
    Q_ASSERT(id != qnCommon->moduleGUID());
    checkAndAddAddress(address, port, id, &m_autoUrls);
}

void QnDirectModuleFinder::removeAddress(const QHostAddress &address, quint16 port, const QnId &id) {
    m_autoUrls.remove(makeRequestUrl(address, port), id);
}

void QnDirectModuleFinder::addManualUrl(const QUrl &url, const QnId &id) {
    Q_ASSERT(id != qnCommon->moduleGUID());
    checkAndAddUrl(url, id, &m_manualUrls);
}

void QnDirectModuleFinder::removeManualUrl(const QUrl &url, const QnId &id) {
    m_manualUrls.remove(makeRequestUrl(url), id);
}

void QnDirectModuleFinder::addManualAddress(const QHostAddress &address, quint16 port, const QnId &id) {
    Q_ASSERT(id != qnCommon->moduleGUID());
    checkAndAddAddress(address, port, id, &m_manualUrls);
}

void QnDirectModuleFinder::removeManualAddress(const QHostAddress &address, quint16 port, const QnId &id) {
    m_manualUrls.remove(makeRequestUrl(address, port), id);
}

void QnDirectModuleFinder::addIgnoredModule(const QUrl &url, const QnId &id) {
    checkAndAddUrl(url, id, &m_ignoredModules);
}

void QnDirectModuleFinder::removeIgnoredModule(const QUrl &url, const QnId &id) {
    m_ignoredModules.remove(makeRequestUrl(url), id);
}

void QnDirectModuleFinder::addIgnoredModule(const QHostAddress &address, quint16 port, const QnId &id) {
    checkAndAddAddress(address, port, id, &m_ignoredModules);
}

void QnDirectModuleFinder::removeIgnoredModule(const QHostAddress &address, quint16 port, const QnId &id) {
    m_ignoredModules.remove(makeRequestUrl(address, port), id);
}

void QnDirectModuleFinder::addIgnoredUrl(const QUrl &url) {
    m_ignoredUrls.insert(makeRequestUrl(url));
}

void QnDirectModuleFinder::removeIgnoredUrl(const QUrl &url) {
    m_ignoredUrls.remove(makeRequestUrl(url));
}

void QnDirectModuleFinder::addIgnoredAddress(const QHostAddress &address, quint16 port) {
    m_ignoredUrls.insert(makeRequestUrl(address, port));
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
    foreach (const QUrl &url, m_manualUrls.keys())
        enqueRequest(url);

    foreach (const QUrl &url, m_autoUrls.keys())
        enqueRequest(url);

    m_manualCheckTimer->start();
}

void QnDirectModuleFinder::stop() {
    m_manualCheckTimer->stop();
    m_requestQueue.clear();
}

QList<QnModuleInformation> QnDirectModuleFinder::foundModules() const {
    return m_foundModules.values();
}

QnModuleInformation QnDirectModuleFinder::moduleInformation(const QnId &id) const {
    return m_foundModules[id];
}

QMultiHash<QUrl, QnId> QnDirectModuleFinder::autoUrls() const {
    return m_autoUrls;
}

QMultiHash<QUrl, QnId> QnDirectModuleFinder::manualUrls() const {
    return m_manualUrls;
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

    if (m_ignoredUrls.contains(url))
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

        QnIdSet expectedIds = expectedModuleIds(url);
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
        emit moduleFound(moduleInformation, url.host());
    } else {
        QnId id = m_moduleByUrl[url];
        if (id.isNull())
            return;

        if (m_lastPingById[id] + maxPingTimeoutMs < QDateTime::currentMSecsSinceEpoch()) {
            m_lastPingById.remove(id);
            QnModuleInformation moduleInformation = m_foundModules.take(id);
            foreach (const QString &address, moduleInformation.remoteAddresses)
                m_moduleByUrl.remove(makeRequestUrl(QHostAddress(address), moduleInformation.port));

            emit moduleLost(moduleInformation);
        }
    }
}

void QnDirectModuleFinder::at_manualCheckTimer_timeout() {
    foreach (const QUrl &url, m_manualUrls.keys())
        enqueRequest(url);
}
