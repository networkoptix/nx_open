#include "direct_module_finder.h"

#include <QtCore/QTimer>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

#include <utils/common/model_functions.h>
#include <rest/server/json_rest_result.h>

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

void QnDirectModuleFinder::checkAndAddUrl(const QUrl &srcUrl, QSet<QUrl> *urlSet) {
    QUrl url = makeRequestUrl(srcUrl);
    if (!urlSet->contains(url)) {
        urlSet->insert(url);
        enqueRequest(url);
    }
}

void QnDirectModuleFinder::checkAndAddAddress(const QHostAddress &address, quint16 port, QSet<QUrl> *urlSet) {
    QUrl url = makeRequestUrl(address, port);
    if (!urlSet->contains(url)) {
        urlSet->insert(url);
        enqueRequest(url);
    }
}

void QnDirectModuleFinder::addUrl(const QUrl &url) {
    checkAndAddUrl(url, &m_autoUrls);
}

void QnDirectModuleFinder::removeUrl(const QUrl &url) {
    m_autoUrls.remove(makeRequestUrl(url));
}

void QnDirectModuleFinder::addAddress(const QHostAddress &address, quint16 port) {
    checkAndAddAddress(address, port, &m_autoUrls);
}

void QnDirectModuleFinder::removeAddress(const QHostAddress &address, quint16 port) {
    m_autoUrls.remove(makeRequestUrl(address, port));
}

void QnDirectModuleFinder::addManualUrl(const QUrl &url) {
    checkAndAddUrl(url, &m_manualUrls);
}

void QnDirectModuleFinder::removeManualUrl(const QUrl &url) {
    m_manualUrls.remove(makeRequestUrl(url));
}

void QnDirectModuleFinder::addManualAddress(const QHostAddress &address, quint16 port) {
    checkAndAddAddress(address, port, &m_manualUrls);
}

void QnDirectModuleFinder::removeManualAddress(const QHostAddress &address, quint16 port) {
    m_manualUrls.remove(makeRequestUrl(address, port));
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

void QnDirectModuleFinder::addIgnoredModule(const QnId &id, const QUrl &url) {
    m_ignoredModules.insert(id, makeRequestUrl(url));
}

void QnDirectModuleFinder::removeIgnoredModule(const QnId &id, const QUrl &url) {
    m_ignoredModules.remove(id, makeRequestUrl(url));
}

void QnDirectModuleFinder::addIgnoredModule(const QnId &id, const QHostAddress &address, quint16 port) {
    m_ignoredModules.insert(id, makeRequestUrl(address, port));
}

void QnDirectModuleFinder::removeIgnoredModule(const QnId &id, const QHostAddress &address, quint16 port) {
    m_ignoredModules.remove(id, makeRequestUrl(address, port));
}

void QnDirectModuleFinder::checkUrl(const QUrl &url) {
    QUrl fixedUrl = url;
    fixedUrl.setPath(lit("/api/moduleInformation"));
    enqueRequest(fixedUrl);
}

void QnDirectModuleFinder::start() {
    foreach (const QUrl &url, m_manualUrls)
        enqueRequest(url);

    foreach (const QUrl &url, m_autoUrls)
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

QSet<QUrl> QnDirectModuleFinder::autoUrls() const {
    return m_autoUrls;
}

QSet<QUrl> QnDirectModuleFinder::ignoredUrls() const {
    return m_ignoredUrls;
}

QSet<QUrl> QnDirectModuleFinder::manualUrls() const {
    return m_manualUrls;
}

QMultiHash<QnId, QUrl> QnDirectModuleFinder::ignoredModules() const {
    return m_ignoredModules;
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

        if (m_ignoredModules.contains(moduleInformation.id, url))
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
    foreach (const QUrl &url, m_manualUrls)
        enqueRequest(url);
}
