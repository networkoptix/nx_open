#include "module_finder.h"

#include "utils/common/log.h"
#include "multicast_module_finder.h"
#include "direct_module_finder.h"
#include "direct_module_finder_helper.h"
#include "common/common_module.h"
#include "core/resource/media_server_resource.h"
#include "core/resource_management/resource_pool.h"

namespace {
    const int pingTimeout = 15 * 1000;
    const int checkInterval = 3000;

    QUrl trimmedUrl(const QUrl &url) {
        QUrl newUrl;
        newUrl.setScheme(lit("http"));
        newUrl.setHost(url.host());
        newUrl.setPort(url.port());
        return newUrl;
    }
}

QnModuleFinder::QnModuleFinder(bool clientOnly) :
    m_timer(new QTimer(this)),
    m_multicastModuleFinder(new QnMulticastModuleFinder(clientOnly)),
    m_directModuleFinder(new QnDirectModuleFinder(this)),
    m_helper(new QnDirectModuleFinderHelper(this))
{
    connect(m_multicastModuleFinder.data(), &QnMulticastModuleFinder::responseRecieved,     this, &QnModuleFinder::at_responseRecieved);
    connect(m_directModuleFinder,           &QnDirectModuleFinder::responseRecieved,        this, &QnModuleFinder::at_responseRecieved);

    m_timer->setInterval(checkInterval);
    connect(m_timer, &QTimer::timeout, this, &QnModuleFinder::at_timer_timeout);
}

QnModuleFinder::~QnModuleFinder() {
    pleaseStop();
}

void QnModuleFinder::setCompatibilityMode(bool compatibilityMode) {
    m_multicastModuleFinder->setCompatibilityMode(compatibilityMode);
    m_directModuleFinder->setCompatibilityMode(compatibilityMode);
}

bool QnModuleFinder::isCompatibilityMode() const {
    return m_multicastModuleFinder->isCompatibilityMode();
}

QList<QnModuleInformation> QnModuleFinder::foundModules() const {
    return m_foundModules.values();
}

QnModuleInformation QnModuleFinder::moduleInformation(const QnUuid &moduleId) const {
    return m_foundModules.value(moduleId);
}

QnMulticastModuleFinder *QnModuleFinder::multicastModuleFinder() const {
    return m_multicastModuleFinder.data();
}

QnDirectModuleFinder *QnModuleFinder::directModuleFinder() const {
    return m_directModuleFinder;
}

int QnModuleFinder::pingTimeout() const {
    return ::pingTimeout;
}

void QnModuleFinder::start() {
    m_multicastModuleFinder->start();
    m_directModuleFinder->start();
    m_elapsedTimer.start();
    m_timer->start();
}

void QnModuleFinder::pleaseStop() {
    m_timer->stop();
    m_multicastModuleFinder->pleaseStop();
    m_directModuleFinder->pleaseStop();
}

void QnModuleFinder::at_responseRecieved(QnModuleInformation moduleInformation, QUrl url) {
    if (!qnCommon->allowedPeers().isEmpty() && !qnCommon->allowedPeers().contains(moduleInformation.id))
        return;

    url = trimmedUrl(url);

    QnUuid oldId = m_idByUrl.value(url);
    if (!oldId.isNull() && oldId != moduleInformation.id)
        removeUrl(url);

    m_lastResponse[url] = m_elapsedTimer.elapsed();
    m_idByUrl[url] = moduleInformation.id;
    if (!m_urlById.contains(moduleInformation.id, url))
        m_urlById.insert(moduleInformation.id, url);

    QnModuleInformation &oldInformation = m_foundModules[moduleInformation.id];
    moduleInformation.remoteAddresses = moduleAddresses(moduleInformation.id);

    if (oldInformation != moduleInformation) {
        QSet<QString> oldAddresses = oldInformation.remoteAddresses;
        oldInformation = moduleInformation;

        NX_LOG(lit("QnModuleFinder. Module %1 is changed.").arg(moduleInformation.id.toString()), cl_logDEBUG1);
        emit moduleChanged(moduleInformation);

        for (const QString &address: moduleInformation.remoteAddresses - oldAddresses) {
            NX_LOG(lit("QnModuleFinder: New module URL: %1 %2:%3")
                   .arg(moduleInformation.id.toString()).arg(address).arg(moduleInformation.port), cl_logDEBUG1);

            QUrl url;
            url.setScheme(lit("http"));
            url.setHost(address);
            url.setPort(moduleInformation.port);
            emit moduleUrlFound(moduleInformation, url);
        }
    }
}

void QnModuleFinder::at_timer_timeout() {
    qint64 currentTime = m_elapsedTimer.elapsed();

    /* Copy it because we could remove urls from the hash during the check */
    QHash<QUrl, qint64> lastResponse = m_lastResponse;
    for (auto it = lastResponse.begin(); it != lastResponse.end(); ++it) {
        if (currentTime - it.value() < ::pingTimeout)
            continue;
        removeUrl(it.key());
    }
}

QSet<QString> QnModuleFinder::moduleAddresses(const QnUuid &id) const {
    QSet<QString> result;

    for (const QUrl &url: m_urlById.values(id))
        result.insert(url.host());

    return result;
}

void QnModuleFinder::removeUrl(const QUrl &url) {
    QnUuid id = m_idByUrl.take(url);
    if (id.isNull())
        return;

    m_urlById.remove(id, url);
    m_lastResponse.remove(url);

    auto it = m_foundModules.find(id);
    Q_ASSERT_X(it != m_foundModules.end(), Q_FUNC_INFO, "Module information must exist here.");

    if (it->remoteAddresses.remove(url.host())) {
        NX_LOG(lit("QnModuleFinder: Module URL lost: %1 %2:%3")
               .arg(it->id.toString()).arg(url.host()).arg(it->port), cl_logDEBUG1);
        emit moduleUrlLost(*it, url);
    }

    if (it->remoteAddresses.isEmpty()) {
        NX_LOG(lit("QnModuleFinder: Module %1 lost.").arg(it->id.toString()), cl_logDEBUG1);
        QnModuleInformation moduleInformation = *it;
        m_foundModules.erase(it);
        emit moduleLost(moduleInformation);
    } else {
        NX_LOG(lit("QnModuleFinder: Module %1 is changed.").arg(it->id.toString()), cl_logDEBUG1);
        emit moduleChanged(*it);
    }
}
