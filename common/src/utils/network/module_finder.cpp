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
#include "api/app_server_connection.h"
#include "nx_ec/dummy_handler.h"

namespace {
    const int pingTimeout = 15 * 1000;
    const int checkInterval = 3000;
    const int noticeableConflictCount = 5;

    bool isLocalAddress(const HostAddress &address) {
        if (address.toString().isNull())
            return false;

        quint8 hi = address.ipv4() >> 24;

        return address == HostAddress::localhost || hi == 10 || hi == 192 || hi == 172;
    }

    bool isBetterAddress(const HostAddress &address, const HostAddress &other) {
        return !isLocalAddress(other) && isLocalAddress(address);
    }

    QUrl addressToUrl(const HostAddress &address, int port = -1) {
        QUrl url;
        url.setScheme(lit("http"));
        url.setHost(address.toString());
        url.setPort(port);
        return url;
    }
}

QnModuleFinder::QnModuleFinder(bool clientOnly) :
    m_elapsedTimer(),
    m_timer(new QTimer(this)),
    m_clientOnly(clientOnly),
    m_multicastModuleFinder(new QnMulticastModuleFinder(clientOnly)),
    m_directModuleFinder(new QnDirectModuleFinder(this)),
    m_helper(new QnDirectModuleFinderHelper(this))
{
    connect(m_multicastModuleFinder.data(), &QnMulticastModuleFinder::responseReceived,     this, &QnModuleFinder::at_responseReceived);
    connect(m_directModuleFinder,           &QnDirectModuleFinder::responseReceived,        this, &QnModuleFinder::at_responseReceived);

    m_timer->setInterval(checkInterval);
    connect(m_timer, &QTimer::timeout, this, &QnModuleFinder::at_timer_timeout);


    connect(qnResPool, &QnResourcePool::resourceAdded, this, [this](const QnResourcePtr &resource) {
        QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
        if (!server)
            return;
        connect(server.data(), &QnMediaServerResource::auxUrlsChanged, this, &QnModuleFinder::at_server_auxUrlsChanged);
    });
    connect(qnResPool, &QnResourcePool::resourceRemoved, this, [this](const QnResourcePtr &resource) {
        QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
        if (!server)
            return;
        disconnect(server.data(), &QnMediaServerResource::auxUrlsChanged, this, &QnModuleFinder::at_server_auxUrlsChanged);
    });
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
    for (const ModuleItem &moduleItem: m_moduleItemById)
        result.append(moduleItem.moduleInformation);
    return result;
}

QnModuleInformation QnModuleFinder::moduleInformation(const QnUuid &moduleId) const {
    return m_moduleItemById.value(moduleId).moduleInformation;
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

void QnModuleFinder::at_responseReceived(const QnModuleInformation &moduleInformation, const SocketAddress &address) {
    if (!qnCommon->allowedPeers().isEmpty() && !qnCommon->allowedPeers().contains(moduleInformation.id))
        return;

    if (moduleInformation.id == qnCommon->moduleGUID()) {
        handleSelfResponse(moduleInformation, address);
        return;
    }

    QnUuid oldId = m_idByAddress.value(address);
    if (!oldId.isNull() && oldId != moduleInformation.id)
        removeAddress(address, true);

    QnMediaServerResourcePtr server = qnResPool->getResourceById(moduleInformation.id).dynamicCast<QnMediaServerResource>();
    if (server && (server->getIgnoredUrls().contains(addressToUrl(address.address, address.port)) ||
                   server->getIgnoredUrls().contains(addressToUrl(address.address))))
    {
        return;
    }

    qint64 currentTime = m_elapsedTimer.elapsed();

    m_lastResponse[address] = currentTime;

    ModuleItem &item = m_moduleItemById[moduleInformation.id];

    /* Handle conflicting servers */
    if (!item.moduleInformation.id.isNull() && item.moduleInformation.runtimeId != moduleInformation.runtimeId) {
        bool oldModuleIsValid = item.moduleInformation.systemName == qnCommon->localSystemName();
        bool newModuleIsValid = moduleInformation.systemName == qnCommon->localSystemName();

        if (oldModuleIsValid == newModuleIsValid) {
            oldModuleIsValid = item.moduleInformation.customization == QnAppInfo::customizationName();
            newModuleIsValid = moduleInformation.customization == QnAppInfo::customizationName();
        }

        if (oldModuleIsValid == newModuleIsValid) {
            QnUuid remoteId = qnCommon->remoteGUID();
            if (!remoteId.isNull() && remoteId == moduleInformation.id) {
                QnUuid correctRuntimeId = QnRuntimeInfoManager::instance()->item(remoteId).uuid;
                oldModuleIsValid = item.moduleInformation.runtimeId == correctRuntimeId;
                newModuleIsValid = moduleInformation.runtimeId == correctRuntimeId;
            }
        }

        if (!newModuleIsValid || oldModuleIsValid) {
            if (currentTime - item.lastConflictResponse < pingTimeout()) {
                if (item.lastResponse >= item.lastConflictResponse)
                    ++item.conflictResponseCount;
            } else {
                item.conflictResponseCount = 0;
            }
            item.lastConflictResponse = currentTime;

            if (item.conflictResponseCount >= noticeableConflictCount && item.conflictResponseCount % noticeableConflictCount == 0) {
                NX_LOG(lit("QnModuleFinder: Server %1 conflict: %2")
                       .arg(moduleInformation.id.toString()).arg(address.toString()), cl_logWARNING);
            }

            return;
        }

        item.primaryAddress = SocketAddress();
        foreach (const SocketAddress &address, item.addresses)
            removeAddress(address, true);
    }

    if (item.moduleInformation != moduleInformation) {
        NX_LOG(lit("QnModuleFinder. Module %1 is changed.").arg(moduleInformation.id.toString()), cl_logDEBUG1);
        emit moduleChanged(moduleInformation);

        if (item.moduleInformation.port != moduleInformation.port) {
            item.primaryAddress = SocketAddress();
            foreach (const SocketAddress &address, item.addresses) {
                if (address.port == item.moduleInformation.port)
                    removeAddress(address, true);
            }
        }

        item.moduleInformation = moduleInformation;
        if (item.primaryAddress.port == 0)
            item.primaryAddress = address;

        sendModuleInformation(moduleInformation, address, true);
    }

    item.lastResponse = currentTime;

    int count = item.addresses.size();
    item.addresses.insert(address);
    m_idByAddress[address] = moduleInformation.id;
    if (count < item.addresses.size()) {
        if (isBetterAddress(address.address, item.primaryAddress.address)) {
            item.primaryAddress = address;
            sendModuleInformation(moduleInformation, address, true);
        }

        NX_LOG(lit("QnModuleFinder: New module URL: %1 %2")
               .arg(moduleInformation.id.toString()).arg(address.toString()), cl_logDEBUG1);

        emit moduleAddressFound(moduleInformation, address);
    }
}

void QnModuleFinder::at_timer_timeout() {
    qint64 currentTime = m_elapsedTimer.elapsed();

    QList<SocketAddress> addressesToRemove;

    for (auto it = m_lastResponse.begin(); it != m_lastResponse.end(); ++it) {
        if (currentTime - it.value() < pingTimeout())
            continue;
        addressesToRemove.append(it.key());
    }

    for (const SocketAddress &address: addressesToRemove)
        removeAddress(address, false);
}

void QnModuleFinder::at_server_auxUrlsChanged(const QnResourcePtr &resource) {
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    Q_ASSERT_X(!server.isNull(), Q_FUNC_INFO, "server resource is expected");
    if (!server)
        return;

    int port = server->getPort();
    for (const QUrl &url: server->getIgnoredUrls())
        removeAddress(SocketAddress(url.host(), url.port(port)), false);
}

QSet<SocketAddress> QnModuleFinder::moduleAddresses(const QnUuid &id) const {
    return m_moduleItemById.value(id).addresses;
}

SocketAddress QnModuleFinder::primaryAddress(const QnUuid &id) const {
    return m_moduleItemById.value(id).primaryAddress;
}

void QnModuleFinder::removeAddress(const SocketAddress &address, bool holdItem) {
    QnUuid id = m_idByAddress.take(address);
    if (id.isNull())
        return;

    auto it = m_moduleItemById.find(id);
    if (it == m_moduleItemById.end())
        return;

    if (!it->addresses.remove(address))
        return;

    QnModuleInformation &moduleInformation = it->moduleInformation;

    NX_LOG(lit("QnModuleFinder: Module URL lost: %1 %2:%3")
           .arg(moduleInformation.id.toString()).arg(address.address.toString()).arg(moduleInformation.port), cl_logDEBUG1);
    emit moduleAddressLost(moduleInformation, address);

    if (!it->addresses.isEmpty()) {
        if (it->primaryAddress == address) {
            it->primaryAddress.port = 0;
            for (const SocketAddress &address: it->addresses) {
                if (isLocalAddress(address.address)) {
                    it->primaryAddress = address;
                    break;
                }
            }
            if (it->primaryAddress.port == 0)
                it->primaryAddress = *it->addresses.cbegin();
            sendModuleInformation(moduleInformation, it->primaryAddress, true);
        }

        return;
    }

    NX_LOG(lit("QnModuleFinder: Module %1 is lost.").arg(moduleInformation.id.toString()), cl_logDEBUG1);

    QnModuleInformation moduleInformationCopy = moduleInformation;
    SocketAddress primaryAddress = it->primaryAddress;

    if (holdItem)
        moduleInformation.id = QnUuid();
    else
        m_moduleItemById.erase(it);

    emit moduleLost(moduleInformationCopy);

    sendModuleInformation(moduleInformationCopy, primaryAddress, false);
}

void QnModuleFinder::handleSelfResponse(const QnModuleInformation &moduleInformation, const SocketAddress &address) {
    QnModuleInformation current = qnCommon->moduleInformation();
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
        emit moduleConflict(moduleInformation, address);
}

void QnModuleFinder::sendModuleInformation(const QnModuleInformation &moduleInformation, const SocketAddress &address, bool isAlive) {
    if (m_clientOnly)
        return;

    QnModuleInformationWithAddresses moduleInformationWithAddresses(moduleInformation);
    moduleInformationWithAddresses.remoteAddresses.insert(address.address.toString());
    moduleInformationWithAddresses.port = address.port;
    QnAppServerConnectionFactory::getConnection2()->getMiscManager()->sendModuleInformation(
                std::move(moduleInformationWithAddresses), isAlive,
                ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
}
