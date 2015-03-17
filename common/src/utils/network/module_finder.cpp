#include "module_finder.h"

#include <QtCore/QCryptographicHash>

#include "utils/common/log.h"
#include "multicast_module_finder.h"
#include "direct_module_finder.h"
#include "direct_module_finder_helper.h"
#include "common/common_module.h"
#include "core/resource/media_server_resource.h"
#include "core/resource_management/resource_pool.h"
#include "utils/common/app_info.h"
#include "api/runtime_info_manager.h"

namespace {
    const int pingTimeout = 15 * 1000;
    const int checkInterval = 3000;
    const int noticeableConflictCount = 5;

    QUrl trimmedUrl(const QUrl &url) {
        QUrl newUrl;
        newUrl.setScheme(lit("http"));
        newUrl.setHost(url.host());
        newUrl.setPort(url.port());
        return newUrl;
    }

    QUrl makeUrl(const QString &host, int port) {
        QUrl url;
        url.setScheme(lit("http"));
        url.setHost(host);
        url.setPort(port);
        return url;
    }
}

QnModuleFinder::QnModuleFinder(bool clientOnly) :
    m_elapsedTimer(),
    m_timer(new QTimer(this)),
    m_multicastModuleFinder(new QnMulticastModuleFinder(clientOnly)),
    m_directModuleFinder(new QnDirectModuleFinder(this)),
    m_helper(new QnDirectModuleFinderHelper(this))
{
    connect(m_multicastModuleFinder.data(), &QnMulticastModuleFinder::responseReceived,     this, &QnModuleFinder::at_responseReceived);
    connect(m_directModuleFinder,           &QnDirectModuleFinder::responseReceived,        this, &QnModuleFinder::at_responseReceived);

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
    QList<QnModuleInformation> result;
    for (const QnModuleInformation &moduleInformation: m_foundModules)
        result.append(moduleInformation);
    return result;
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

QnDirectModuleFinderHelper *QnModuleFinder::directModuleFinderHelper() const {
    return m_helper;
}

int QnModuleFinder::pingTimeout() const {
    return ::pingTimeout;
}

void QnModuleFinder::start() {
    m_lastSelfConflict = 0;
    m_selfConflictCount = 0;
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

void QnModuleFinder::at_responseReceived(QnModuleInformationEx moduleInformation, QUrl url) {
    if (!qnCommon->allowedPeers().isEmpty() && !qnCommon->allowedPeers().contains(moduleInformation.id))
        return;

    if (moduleInformation.id == qnCommon->moduleGUID()) {
        handleSelfResponse(moduleInformation, url);
        return;
    }

    url = trimmedUrl(url);

    QnUuid oldId = m_idByUrl.value(url);
    if (!oldId.isNull() && oldId != moduleInformation.id)
        removeUrl(url);

    qint64 currentTime = m_elapsedTimer.elapsed();

    m_lastResponse[url] = currentTime;

    QnModuleInformationEx &oldInformation = m_foundModules[moduleInformation.id];

    /* Handle conflicting servers */
    if (!oldInformation.id.isNull() && oldInformation.runtimeId != moduleInformation.runtimeId) {
        bool oldModuleIsValid = oldInformation.systemName == qnCommon->localSystemName();
        bool newModuleIsValid = moduleInformation.systemName == qnCommon->localSystemName();

        if (oldModuleIsValid == newModuleIsValid) {
            oldModuleIsValid = oldInformation.customization == QnAppInfo::customizationName();
            newModuleIsValid = moduleInformation.customization == QnAppInfo::customizationName();
        }

        if (oldModuleIsValid == newModuleIsValid) {
            QnUuid remoteId = qnCommon->remoteGUID();
            if (!remoteId.isNull() && remoteId == moduleInformation.id) {
                QnUuid correctRuntimeId = QnRuntimeInfoManager::instance()->item(remoteId).uuid;
                oldModuleIsValid = oldInformation.runtimeId == correctRuntimeId;
                newModuleIsValid = moduleInformation.runtimeId == correctRuntimeId;
            }
        }

        if (newModuleIsValid && !oldModuleIsValid) {
            QSet<QString> oldAddresses = oldInformation.remoteAddresses;
            for (const QString &address: oldAddresses)
                removeUrl(makeUrl(address, oldInformation.port));

            addUrl(url, moduleInformation.id);

            moduleInformation.remoteAddresses.clear();
            moduleInformation.remoteAddresses.insert(url.host());
            m_foundModules.insert(moduleInformation.id, moduleInformation);
            NX_LOG(lit("QnModuleFinder. Module %1 is changed.").arg(moduleInformation.id.toString()), cl_logDEBUG1);
            emit moduleChanged(moduleInformation);
            NX_LOG(lit("QnModuleFinder: New module URL: %1 %2:%3")
                   .arg(moduleInformation.id.toString()).arg(url.host()).arg(moduleInformation.port), cl_logDEBUG1);
            emit moduleUrlFound(moduleInformation, url);
            return;
        }

        qint64 &lastConflictResponse = m_lastConflictResponseById[oldInformation.id];
        qint64 lastResponse = m_lastResponseById[oldInformation.id];
        int &conflictCount = m_conflictResponseCountById[oldInformation.id];

        if (currentTime - lastConflictResponse < pingTimeout()) {
            if (lastResponse >= lastConflictResponse)
                ++conflictCount;
        } else {
            conflictCount = 0;
        }
        lastConflictResponse = currentTime;

        if (conflictCount >= noticeableConflictCount && conflictCount % noticeableConflictCount == 0) {
            NX_LOG(lit("QnModuleFinder: Server %1 conflict: %2:%3")
                   .arg(moduleInformation.id.toString()).arg(url.host()).arg(moduleInformation.port), cl_logWARNING);
        }

        return;
    }

    addUrl(url, moduleInformation.id);
    moduleInformation.remoteAddresses = moduleAddresses(moduleInformation.id);

    if (oldInformation != moduleInformation) {
        NX_LOG(lit("QnModuleFinder. Module %1 is changed.").arg(moduleInformation.id.toString()), cl_logDEBUG1);
        emit moduleChanged(moduleInformation);

        QSet<QString> oldAddresses = oldInformation.remoteAddresses;
        if (oldInformation.port != moduleInformation.port) {
            for (const QString &address: oldAddresses)
                removeUrl(makeUrl(address, oldInformation.port));

            oldAddresses.clear();
            moduleInformation.remoteAddresses.clear();
            moduleInformation.remoteAddresses.insert(url.host());
            m_foundModules.insert(moduleInformation.id, moduleInformation);
        } else {
            oldInformation = moduleInformation;
        }

        for (const QString &address: moduleInformation.remoteAddresses - oldAddresses) {
            NX_LOG(lit("QnModuleFinder: New module URL: %1 %2:%3")
                   .arg(moduleInformation.id.toString()).arg(address).arg(moduleInformation.port), cl_logDEBUG1);

            emit moduleUrlFound(moduleInformation, url);
        }
    }

    m_lastResponseById[moduleInformation.id] = currentTime;
}

void QnModuleFinder::at_timer_timeout() {
    qint64 currentTime = m_elapsedTimer.elapsed();

    /* Copy it because we could remove urls from the hash during the check */
    QHash<QUrl, qint64> lastResponse = m_lastResponse;
    for (auto it = lastResponse.begin(); it != lastResponse.end(); ++it) {
        if (currentTime - it.value() < pingTimeout())
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
    if (it == m_foundModules.end())
        return;

    if (it->remoteAddresses.remove(url.host())) {
        NX_LOG(lit("QnModuleFinder: Module URL lost: %1 %2:%3")
               .arg(it->id.toString()).arg(url.host()).arg(it->port), cl_logDEBUG1);
        emit moduleUrlLost(*it, url);
    }

    if (it->remoteAddresses.isEmpty()) {
        NX_LOG(lit("QnModuleFinder: Module %1 is lost.").arg(it->id.toString()), cl_logDEBUG1);
        QnModuleInformation moduleInformation = *it;
        m_foundModules.erase(it);
        emit moduleLost(moduleInformation);
    } else {
        NX_LOG(lit("QnModuleFinder: Module %1 is changed.").arg(it->id.toString()), cl_logDEBUG1);
        emit moduleChanged(*it);
    }
}

void QnModuleFinder::addUrl(const QUrl &url, const QnUuid &id) {
    m_idByUrl[url] = id;
    if (!m_urlById.contains(id, url))
        m_urlById.insert(id, url);
}

void QnModuleFinder::handleSelfResponse(const QnModuleInformationEx &moduleInformation, const QUrl &url) {
    QnModuleInformationEx current = qnCommon->moduleInformation();
    if (current.runtimeId == moduleInformation.runtimeId)
        return;

    qint64 currentTime = m_elapsedTimer.elapsed();
    if (currentTime - m_lastSelfConflict > pingTimeout()) {
        m_selfConflictCount = 1;
        m_lastSelfConflict = currentTime;
        return;
    }
    m_lastSelfConflict = currentTime;
    ++m_selfConflictCount;

    if (m_selfConflictCount >= noticeableConflictCount && m_selfConflictCount % noticeableConflictCount == 0)
        emit moduleConflict(moduleInformation, url);
}
