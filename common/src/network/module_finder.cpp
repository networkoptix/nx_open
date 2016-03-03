#include "module_finder.h"

#include <QtCore/QCryptographicHash>

#include <api/global_settings.h>
#include <nx/utils/log/log.h>
#include <nx/network/socket_global.h>
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

namespace
{
    const int kCheckInterval = 3000;
    const int kNoticeableConflictCount = 5;

    bool isLocalAddress(const HostAddress &address)
    {
        if (address.toString().isNull())
            return false;

        quint8 hi = address.ipv4() >> 24;

        return address == HostAddress::localhost || hi == 10 || hi == 192 || hi == 172;
    }

    bool isBetterAddress(const HostAddress &address, const HostAddress &other)
    {
        return !isLocalAddress(other) && isLocalAddress(address);
    }

    QUrl addressToUrl(const HostAddress &address, int port = -1)
    {
        QUrl url;
        url.setScheme(lit("http"));
        url.setHost(address.toString());
        url.setPort(port);
        return url;
    }

    SocketAddress pickPrimaryAddress(QSet<SocketAddress> addresses, const QSet<QUrl> &ignoredUrls)
    {
        if (addresses.isEmpty())
            return SocketAddress();

        SocketAddress result;

        for (const SocketAddress &address: addresses) {
            if (ignoredUrls.contains(addressToUrl(address.address, address.port)) ||
                ignoredUrls.contains(addressToUrl(address.address)))
            {
                continue;
            }

            if (isLocalAddress(address.address))
                return address;

            if (result.port == 0)
                result = address;
        }

        return result;
    }

    QSet<QUrl> ignoredUrlsForServer(const QnUuid &id)
    {
        QnMediaServerResourcePtr server = qnResPool->getResourceById<QnMediaServerResource>(id);
        if (!server)
            return QSet<QUrl>();

        QSet<QUrl> result = QSet<QUrl>::fromList(server->getIgnoredUrls());

        /* Currently used EC URL should be non-ignored. */
        if (id == qnCommon->remoteGUID()) {
            QUrl ecUrl = QnAppServerConnectionFactory::getConnection2()->connectionInfo().ecUrl;

            QUrl url;
            url.setScheme(lit("http"));
            url.setHost(ecUrl.host());

            result.remove(url);

            url.setPort(ecUrl.port());
            result.remove(url);
        }

        return result;
    }

    Qn::ResourceStatus calculateModuleStatus(
            const QnModuleInformation &moduleInformation,
            Qn::ResourceStatus currentStatus = Qn::Online)
    {
        Qn::ResourceStatus status = moduleInformation.isCompatibleToCurrentSystem() ? Qn::Online : Qn::Incompatible;
        if (status == Qn::Online && currentStatus == Qn::Unauthorized)
            status = Qn::Unauthorized;

        return status;
    }
}

QnModuleFinder::QnModuleFinder(bool clientMode, bool compatibilityMode) :
    m_itemsMutex(QnMutex::Recursive),
    m_elapsedTimer(),
    m_timer(new QTimer(this)),
    m_clientMode(clientMode),
    m_multicastModuleFinder(new QnMulticastModuleFinder(clientMode)),
    m_directModuleFinder(new QnDirectModuleFinder(this)),
    m_helper(new QnDirectModuleFinderHelper(this, clientMode))
{
    m_multicastModuleFinder->setCompatibilityMode(compatibilityMode);
    m_directModuleFinder->setCompatibilityMode(compatibilityMode);

    connect(m_multicastModuleFinder.data(), &QnMulticastModuleFinder::responseReceived,     this, &QnModuleFinder::at_responseReceived);
    connect(m_directModuleFinder,           &QnDirectModuleFinder::responseReceived,        this, &QnModuleFinder::at_responseReceived);

    m_timer->setInterval(kCheckInterval);
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

QnModuleFinder::~QnModuleFinder()
{
    pleaseStop();
}

bool QnModuleFinder::isCompatibilityMode() const
{
    return m_multicastModuleFinder->isCompatibilityMode();
}

QnMulticastModuleFinder *QnModuleFinder::multicastModuleFinder() const
{
    return m_multicastModuleFinder.data();
}

QnDirectModuleFinder *QnModuleFinder::directModuleFinder() const
{
    return m_directModuleFinder;
}

QnDirectModuleFinderHelper *QnModuleFinder::directModuleFinderHelper() const
{
    return m_helper;
}

std::chrono::milliseconds QnModuleFinder::pingTimeout() const
{
    // ModuleFinder uses amplified timeout to fix possible temporary problems
    // in QnMulticastModuleFinder (e.g. extend default timeouts 15 sec to 30 sec)
    return QnGlobalSettings::instance()->serverDiscoveryAliveCheckTimeout() * 2;
}

void QnModuleFinder::setModuleStatus(const QnUuid &moduleId, Qn::ResourceStatus status)
{
    QnMutexLocker lk(&m_itemsMutex);
    auto it = m_moduleItemById.find(moduleId);
    if (it == m_moduleItemById.end())
        return;

    switch (status)
    {
    case Qn::Online:
    case Qn::Unauthorized:
    case Qn::Incompatible:
        break;
    case Qn::Offline:
        lk.unlock();
        removeModule(moduleId);
        return;
    default:
        status = Qn::Incompatible;
    }

    if (it->status == status)
        return;

    it->status = status;

    QnModuleInformation moduleInformation = it->moduleInformation;
    SocketAddress primaryAddress = it->primaryAddress;

    lk.unlock();

    sendModuleInformation(moduleInformation, primaryAddress, status);
}

void QnModuleFinder::start()
{
    m_lastSelfConflict = 0;
    m_selfConflictCount = 0;
    m_multicastModuleFinder->start();
    m_directModuleFinder->start();
    m_elapsedTimer.start();
    m_timer->start();
}

void QnModuleFinder::pleaseStop()
{
    m_timer->stop();
    m_multicastModuleFinder->pleaseStop();
    m_directModuleFinder->pleaseStop();
}

QList<QnModuleInformation> QnModuleFinder::foundModules() const
{
    QnMutexLocker lk(&m_itemsMutex);
    QList<QnModuleInformation> result;
    for (const ModuleItem &moduleItem: m_moduleItemById) {
        if (moduleItem.moduleInformation.id.isNull())
            continue;

        result.append(moduleItem.moduleInformation);
    }
    return result;
}

ec2::ApiDiscoveredServerDataList QnModuleFinder::discoveredServers() const
{
    ec2::ApiDiscoveredServerDataList result;

    for (const ModuleItem &moduleItem: m_moduleItemById) {
        ec2::ApiDiscoveredServerData serverData(moduleItem.moduleInformation);

        for (const SocketAddress &address: moduleItem.addresses) {
            if (address.port == serverData.port)
                serverData.remoteAddresses.insert(address.address.toString());
            else
                serverData.remoteAddresses.insert(address.toString());
        }

        serverData.status = moduleItem.status;

        result.push_back(serverData);
    }

    return result;
}

QnModuleInformation QnModuleFinder::moduleInformation(const QnUuid &moduleId) const
{
    QnMutexLocker lk(&m_itemsMutex);
    return m_moduleItemById.value(moduleId).moduleInformation;
}

QSet<SocketAddress> QnModuleFinder::moduleAddresses(const QnUuid &id) const
{
    QnMutexLocker lk(&m_itemsMutex);
    return m_moduleItemById.value(id).addresses;
}

SocketAddress QnModuleFinder::primaryAddress(const QnUuid &id) const
{
    QnMutexLocker lk(&m_itemsMutex);
    return m_moduleItemById.value(id).primaryAddress;
}

Qn::ResourceStatus QnModuleFinder::moduleStaus(const QnUuid &id) const
{
    QnMutexLocker lk(&m_itemsMutex);
    return m_moduleItemById.value(id).status;
}

void QnModuleFinder::at_responseReceived(const QnModuleInformation &moduleInformation, const SocketAddress &address)
{
    if (!qnCommon->allowedPeers().isEmpty() && !qnCommon->allowedPeers().contains(moduleInformation.id))
        return;

    if (moduleInformation.id == qnCommon->moduleGUID()) {
        handleSelfResponse(moduleInformation, address);
        return;
    }

    QnUuid oldId = m_idByAddress.value(address);
    if (!oldId.isNull() && oldId != moduleInformation.id)
    {
        NX_LOG(lit("QnModuleFinder::at_responseReceived. Removing address %1 since peer id mismatch (old %2, new %3)")
            .arg(address.toString()).arg(oldId.toString()).arg(moduleInformation.id.toString()),
            cl_logDEBUG1);
        removeAddress(address, true, ignoredUrlsForServer(oldId));
    }

    bool ignoredAddress = false;
    QSet<QUrl> ignoredUrls = ignoredUrlsForServer(moduleInformation.id);
    if (ignoredUrls.contains(addressToUrl(address.address, address.port)) ||
        ignoredUrls.contains(addressToUrl(address.address)))
    {
        ignoredAddress = true;
    }

    qint64 currentTime = m_elapsedTimer.elapsed();

    QnMutexLocker lk(&m_itemsMutex);

    ModuleItem &item = m_moduleItemById[moduleInformation.id];

    lk.unlock();

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

        QnMutexLocker lk(&m_itemsMutex);

        if (!newModuleIsValid || oldModuleIsValid) {
            if (currentTime - item.lastConflictResponse < pingTimeout().count()) {
                if (item.lastResponse >= item.lastConflictResponse)
                    ++item.conflictResponseCount;
            } else {
                item.conflictResponseCount = 0;
            }
            item.lastConflictResponse = currentTime;

            if (item.conflictResponseCount >= kNoticeableConflictCount && item.conflictResponseCount % kNoticeableConflictCount == 0) {
                NX_LOGX(lit("Server %1 conflict: %2")
                       .arg(moduleInformation.id.toString()).arg(address.toString()), cl_logWARNING);
            }

            return;
        }

        updatePrimaryAddress(item, SocketAddress());

        lk.unlock();

        foreach (const SocketAddress &address, item.addresses)
        {
            NX_LOG(lit("QnModuleFinder::at_responseReceived. Removing address %1 due to server conflict")
                .arg(address.toString()), cl_logDEBUG1);
            removeAddress(address, true);
        }
    }

    m_lastResponse[address] = currentTime;

    if (item.moduleInformation != moduleInformation) {
        NX_LOGX(lit("Module %1 has been changed.").arg(moduleInformation.id.toString()), cl_logDEBUG1);
        emit moduleChanged(moduleInformation);

        if (item.moduleInformation.port != moduleInformation.port) {
            QnMutexLocker lk(&m_itemsMutex);
            updatePrimaryAddress(item, SocketAddress());
            lk.unlock();

            foreach (const SocketAddress &address, item.addresses) {
                if (address.port == item.moduleInformation.port)
                {
                    NX_LOG(lit("QnModuleFinder::at_responseReceived. Removing address %1 due to module information change")
                        .arg(address.toString()), cl_logDEBUG1);
                    removeAddress(address, true);
                }
            }
        }

        QnMutexLocker lk(&m_itemsMutex);

        item.moduleInformation = moduleInformation;
        if (item.primaryAddress.port == 0 && !ignoredAddress)
            updatePrimaryAddress(item, address);

        SocketAddress addressToSend = item.primaryAddress;
        item.status = calculateModuleStatus(item.moduleInformation, item.status);
        Qn::ResourceStatus statusToSend = item.status;

        lk.unlock();

        if (item.primaryAddress.port > 0)
            sendModuleInformation(moduleInformation, addressToSend, statusToSend);
        else
            sendModuleInformation(moduleInformation, address, Qn::Offline);
    }

    lk.relock();

    item.lastResponse = currentTime;

    /* Client needs all addresses including ignored ones because of login dialog. */
    if (ignoredAddress && !m_clientMode)
        return;

    int count = item.addresses.size();
    item.addresses.insert(address);
    m_idByAddress[address] = moduleInformation.id;
    const auto cloudModuleId = moduleInformation.cloudId();
    if (count < item.addresses.size()) {
        if (!ignoredAddress && isBetterAddress(address.address, item.primaryAddress.address)) {
            updatePrimaryAddress(item, address);
            Qn::ResourceStatus status = item.status;

            lk.unlock();

            sendModuleInformation(moduleInformation, address, status);
        }
        else {
            lk.unlock();
        }

        NX_LOGX(lit("New module URL: %1 %2")
               .arg(moduleInformation.id.toString()).arg(address.toString()), cl_logDEBUG1);

        emit moduleAddressFound(moduleInformation, address);

        if (!cloudModuleId.isEmpty())
            nx::network::SocketGlobals::addressResolver().addFixedAddress(
               cloudModuleId, address);
    }
}

void QnModuleFinder::at_timer_timeout()
{
    qint64 currentTime = m_elapsedTimer.elapsed();

    QList<SocketAddress> addressesToRemove;

    for (auto it = m_lastResponse.begin(); it != m_lastResponse.end(); ++it) {
        if (currentTime - it.value() < pingTimeout().count())
            continue;

        addressesToRemove.append(it.key());
    }

    for (const SocketAddress &address: addressesToRemove) {
        QnUuid id = m_idByAddress.value(address);
        QSet<QUrl> ignoredUrls = ignoredUrlsForServer(id);
        NX_LOG(lit("QnModuleFinder::at_timer_timeout. Removing address %1 by timeout")
            .arg(address.toString()), cl_logDEBUG1);
        removeAddress(address, false, ignoredUrls);
    }
}

void QnModuleFinder::at_server_auxUrlsChanged(const QnResourcePtr &resource)
{
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    Q_ASSERT_X(!server.isNull(), Q_FUNC_INFO, "server resource is expected");
    if (!server)
        return;

    int port = server->getPort();
    QSet<QUrl> ignoredUrls = ignoredUrlsForServer(server->getId());
    for (const QUrl &url: server->getIgnoredUrls())
    {
        const SocketAddress addr(url.host(), url.port(port));
        NX_LOG(lit("QnModuleFinder::at_server_auxUrlsChanged. Removing address %1")
            .arg(addr.toString()), cl_logDEBUG1);
        removeAddress(addr, false, ignoredUrls);
    }
}

void QnModuleFinder::removeAddress(const SocketAddress &address, bool holdItem, const QSet<QUrl> &ignoredUrls)
{
    QnUuid id = m_idByAddress.take(address);
    if (id.isNull())
        return;

    auto it = m_moduleItemById.find(id);
    if (it == m_moduleItemById.end())
        return;

    QnMutexLocker lk(&m_itemsMutex);

    if (!it->addresses.remove(address))
        return;

    QnModuleInformation &moduleInformation = it->moduleInformation;

    bool alreadyLost = it->primaryAddress.isNull();

    if (it->primaryAddress == address) 
    {
        updatePrimaryAddress(*it, pickPrimaryAddress(it->addresses, ignoredUrls));
        alreadyLost = it->primaryAddress.isNull();

        SocketAddress addressToSend = it->primaryAddress;
        Qn::ResourceStatus statusToSend = it->status;

        lk.unlock();

        if (it->primaryAddress.port > 0)
            sendModuleInformation(moduleInformation, addressToSend, statusToSend);
        else
            sendModuleInformation(moduleInformation, address, Qn::Offline);
    }
    else
    {
        lk.unlock();
    }

    NX_LOGX(lit("Module URL lost: %1 %2:%3")
           .arg(moduleInformation.id.toString()).arg(address.address.toString()).arg(moduleInformation.port), cl_logDEBUG1);

    emit moduleAddressLost(moduleInformation, address);
    nx::network::SocketGlobals::addressResolver().removeFixedAddress(
        moduleInformation.cloudId(), address);

    if (!it->addresses.isEmpty())
        return;

    NX_LOGX(lit("Module %1 is lost.").arg(moduleInformation.id.toString()), cl_logDEBUG1);

    QnModuleInformation moduleInformationCopy = moduleInformation;
    SocketAddress primaryAddress = it->primaryAddress;

    lk.relock();

    if (holdItem)
        moduleInformation.id = QnUuid();
    else
        m_moduleItemById.erase(it);

    lk.unlock();

    emit moduleLost(moduleInformationCopy);

    if (!alreadyLost)
        sendModuleInformation(moduleInformationCopy, primaryAddress, Qn::Offline);
}

void QnModuleFinder::handleSelfResponse(const QnModuleInformation &moduleInformation, const SocketAddress &address)
{
    QnModuleInformation current = qnCommon->moduleInformation();
    if (current.runtimeId == moduleInformation.runtimeId)
        return;

    qint64 currentTime = m_elapsedTimer.elapsed();
    if (currentTime - m_lastSelfConflict > pingTimeout().count()) {
        m_selfConflictCount = 1;
        m_lastSelfConflict = currentTime;
        return;
    }
    m_lastSelfConflict = currentTime;
    ++m_selfConflictCount;

    if (m_selfConflictCount >= kNoticeableConflictCount && m_selfConflictCount % kNoticeableConflictCount == 0)
        emit moduleConflict(moduleInformation, address);
}

void QnModuleFinder::sendModuleInformation(
        const QnModuleInformation &moduleInformation,
        const SocketAddress &address,
        Qn::ResourceStatus status)
{
    if (m_clientMode)
        return;

    ec2::ApiDiscoveredServerData serverData(moduleInformation);

    serverData.remoteAddresses.insert(address.address.toString());
    serverData.port = address.port;
    serverData.status = status;

    NX_LOG(lit("QnModuleFinder: Send info for %1: %2 -> %3.")
           .arg(moduleInformation.id.toString()).arg(address.toString()).arg(status), cl_logDEBUG2);

    QnAppServerConnectionFactory::getConnection2()->getDiscoveryManager()->sendDiscoveredServer(
                std::move(serverData),
                ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
}

void QnModuleFinder::removeModule(const QnUuid &id)
{
    QList<SocketAddress> addresses;
    {
        QnMutexLocker lock(&m_itemsMutex);

        auto it = m_moduleItemById.find(id);
        if (it == m_moduleItemById.end())
            return;

        addresses = (it->addresses - (QSet<SocketAddress>() << it->primaryAddress)).toList();
        // Place primary address to the end of the list
        addresses.append(it->primaryAddress);
    }

    for (const SocketAddress &address : addresses)
    {
        NX_LOG(lit("QnModuleFinder::removeModule(%1). Removing address %2")
            .arg(id.toString()).arg(address.toString()), cl_logDEBUG1);
        removeAddress(address, false);
    }
}

void QnModuleFinder::updatePrimaryAddress(ModuleItem &item
    , const SocketAddress &address)
{
    if (item.primaryAddress == address)
        return;

    item.primaryAddress = address;
    emit modulePrimaryAddressChanged(item.moduleInformation, address);
}
