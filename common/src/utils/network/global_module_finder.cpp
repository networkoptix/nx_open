#include "global_module_finder.h"

#include <core/resource_management/resource_pool.h>
#include <utils/network/module_finder.h>
#include <utils/common/log.h>
#include <common/common_module.h>
#include <api/app_server_connection.h>
#include <nx_ec/data/api_module_data.h>
#include <nx_ec/dummy_handler.h>

QnGlobalModuleFinder::QnGlobalModuleFinder(QnModuleFinder *moduleFinder, QObject *parent) :
    QObject(parent),
    m_connection(std::weak_ptr<ec2::AbstractECConnection>()),
    m_moduleFinder(moduleFinder)
{
    connect(qnResPool,      &QnResourcePool::statusChanged,         this,       &QnGlobalModuleFinder::at_resourcePool_statusChanged);
    connect(qnResPool,      &QnResourcePool::resourceRemoved,       this,       &QnGlobalModuleFinder::at_resourcePool_resourceRemoved);

    if (moduleFinder) {
        for (const QnModuleInformation &moduleInformation: moduleFinder->foundModules())
            addModule(moduleInformation, qnCommon->moduleGUID());

        connect(moduleFinder,               &QnModuleFinder::moduleChanged, this,   &QnGlobalModuleFinder::at_moduleFinder_moduleChanged);
        connect(moduleFinder,               &QnModuleFinder::moduleLost,    this,   &QnGlobalModuleFinder::at_moduleFinder_moduleLost);
    }
}

void QnGlobalModuleFinder::setConnection(const ec2::AbstractECConnectionPtr &connection) {
    ec2::AbstractECConnectionPtr oldConnection = m_connection.lock();

    if (oldConnection)
        oldConnection->getMiscManager()->disconnect(this);

    QSet<QnUuid> discoverers;
    for (const QSet<QnUuid> &moduleDiscoverer: m_discovererIdByServerId)
        discoverers.unite(moduleDiscoverer);
    discoverers.remove(qnCommon->moduleGUID());

    for (const QnUuid &id: discoverers)
        removeAllModulesDiscoveredBy(id);

    m_connection = connection;

    if (connection)
        connect(connection->getMiscManager().get(), &ec2::AbstractMiscManager::moduleChanged, this, &QnGlobalModuleFinder::at_moduleChanged, Qt::QueuedConnection);
}

void QnGlobalModuleFinder::fillApiModuleData(const QnModuleInformation &moduleInformation, ec2::ApiModuleData *data) {
    data->type = moduleInformation.type;
    data->customization = moduleInformation.customization;
    data->id = moduleInformation.id;
    data->systemName = moduleInformation.systemName;
    data->version = moduleInformation.version.toString();
    data->systemInformation = moduleInformation.systemInformation.toString();
    data->addresses = moduleInformation.remoteAddresses.toList();
    data->port = moduleInformation.port;
    data->name = moduleInformation.name;
    data->authHash = moduleInformation.authHash;
    data->sslAllowed = moduleInformation.sslAllowed;
    data->isAlive = true;
}

void QnGlobalModuleFinder::fillFromApiModuleData(const ec2::ApiModuleData &data, QnModuleInformation *moduleInformation) {
    moduleInformation->type = data.type;
    moduleInformation->customization = data.customization;
    moduleInformation->id = data.id;
    moduleInformation->systemName = data.systemName;
    moduleInformation->version = QnSoftwareVersion(data.version);
    moduleInformation->systemInformation = QnSystemInformation(data.systemInformation);
    moduleInformation->remoteAddresses = QSet<QString>::fromList(data.addresses);
    moduleInformation->port = data.port;
    moduleInformation->name = data.name;
    moduleInformation->authHash = data.authHash;
    moduleInformation->sslAllowed = data.sslAllowed;
}

QList<QnModuleInformation> QnGlobalModuleFinder::foundModules() const {
    return m_moduleInformationById.values();
}

QSet<QnUuid> QnGlobalModuleFinder::discoverers(const QnUuid &moduleId) {
    return m_discovererIdByServerId.value(moduleId);
}

QnModuleInformation QnGlobalModuleFinder::moduleInformation(const QnUuid &id) const {
    return m_moduleInformationById[id];
}

void QnGlobalModuleFinder::at_moduleChanged(const QnModuleInformation &moduleInformation, bool isAlive, const QnUuid &discoverer) {
    if (moduleInformation.id == qnCommon->moduleGUID() || discoverer == qnCommon->moduleGUID())
        return;

    if (isAlive)
        addModule(moduleInformation, discoverer);
    else
        removeModule(moduleInformation, discoverer);
}

void QnGlobalModuleFinder::at_moduleFinder_moduleChanged(const QnModuleInformation &moduleInformation) {
    addModule(moduleInformation, qnCommon->moduleGUID());
    if (ec2::AbstractECConnectionPtr connection = m_connection.lock())
        connection->getMiscManager()->sendModuleInformation(moduleInformation, true, qnCommon->moduleGUID(), ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
}

void QnGlobalModuleFinder::at_moduleFinder_moduleLost(const QnModuleInformation &moduleInformation) {
    removeModule(moduleInformation, qnCommon->moduleGUID());
    if (ec2::AbstractECConnectionPtr connection = m_connection.lock())
        connection->getMiscManager()->sendModuleInformation(moduleInformation, false, qnCommon->moduleGUID(), ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
}

void QnGlobalModuleFinder::at_resourcePool_statusChanged(const QnResourcePtr &resource) {
    if (!resource->hasFlags(Qn::server))
        return;

    if (resource->getStatus() != Qn::Online)
        removeAllModulesDiscoveredBy(resource->getId());
}

void QnGlobalModuleFinder::at_resourcePool_resourceRemoved(const QnResourcePtr &resource) {
    if (!resource->hasFlags(Qn::server))
        return;

    removeAllModulesDiscoveredBy(resource->getId());
}

void QnGlobalModuleFinder::addModule(const QnModuleInformation &moduleInformation, const QnUuid &discoverer) {
    if (moduleInformation.id == qnCommon->moduleGUID())
        return;

    m_discoveredAddresses[moduleInformation.id][discoverer] = moduleInformation.remoteAddresses;

    QnModuleInformation updatedModuleInformation = moduleInformation;
    updatedModuleInformation.remoteAddresses = getModuleAddresses(moduleInformation.id);

    m_discovererIdByServerId[moduleInformation.id].insert(discoverer);

    QnModuleInformation &oldModuleInformation = m_moduleInformationById[moduleInformation.id];
    if (oldModuleInformation != updatedModuleInformation) {
        oldModuleInformation = updatedModuleInformation;
        NX_LOG(lit("QnGlobalModuleFinder. Module %1 is changed, addresses = [%2]")
               .arg(updatedModuleInformation.id.toString())
               .arg(QStringList(QStringList::fromSet(updatedModuleInformation.remoteAddresses)).join(lit(", "))), cl_logDEBUG1);
        emit peerChanged(moduleInformation);
    }
}

void QnGlobalModuleFinder::removeModule(const QnModuleInformation &moduleInformation, const QnUuid &discoverer) {
    if (moduleInformation.id == qnCommon->moduleGUID())
        return;

    QSet<QnUuid> &discoverers = m_discovererIdByServerId[moduleInformation.id];
    if (!discoverers.remove(discoverer))
        return;

    m_discoveredAddresses[moduleInformation.id].remove(discoverer);

    QnModuleInformation updatedModuleInformation = moduleInformation;
    updatedModuleInformation.remoteAddresses = getModuleAddresses(moduleInformation.id);

    if (discoverers.isEmpty()) {
        m_moduleInformationById.remove(updatedModuleInformation.id);
        NX_LOG(lit("QnGlobalModuleFinder. Module %1 is lost.").arg(updatedModuleInformation.id.toString()), cl_logDEBUG1);
        emit peerLost(updatedModuleInformation);
    } else {
        m_moduleInformationById[moduleInformation.id] = updatedModuleInformation;
        NX_LOG(lit("QnGlobalModuleFinder. Module %1 is changed, addresses = [%2]")
               .arg(updatedModuleInformation.id.toString())
               .arg(QStringList(QStringList::fromSet(updatedModuleInformation.remoteAddresses)).join(lit(", "))), cl_logDEBUG1);
        emit peerChanged(updatedModuleInformation);
    }
}

void QnGlobalModuleFinder::removeAllModulesDiscoveredBy(const QnUuid &discoverer) {
    if (discoverer == qnCommon->moduleGUID()) {
        qWarning() << "Trying to remove our own modules";
        return;
    }

    for (auto it = m_moduleInformationById.begin(); it != m_moduleInformationById.end(); /* no inc */) {
        QSet<QnUuid> &discoverers = m_discovererIdByServerId[it.key()];
        if (discoverers.remove(discoverer)) {
            if (discoverers.isEmpty()) {
                NX_LOG(lit("QnGlobalModuleFinder. Module %1 is lost.").arg(it.value().id.toString()), cl_logDEBUG1);
                emit peerLost(it.value());
                it = m_moduleInformationById.erase(it);
                m_discoveredAddresses.remove(it.key());
                continue;
            }
        }
        ++it;
    }

    for (auto it = m_discoveredAddresses.begin(); it != m_discoveredAddresses.end(); /* no inc */) {
        it.value().remove(discoverer);
        if (it.value().isEmpty()) {
            it = m_discoveredAddresses.erase(it);
        } else {
            QnModuleInformation moduleInformation = m_moduleInformationById[it.key()];
            QSet<QString> addresses = getModuleAddresses(moduleInformation.id);
            if (addresses != moduleInformation.remoteAddresses) {
                NX_LOG(lit("QnGlobalModuleFinder. Module %1 is changed, addresses = [%2]")
                       .arg(moduleInformation.id.toString())
                       .arg(QStringList(QStringList::fromSet(moduleInformation.remoteAddresses)).join(lit(", "))), cl_logDEBUG1);
                emit peerChanged(moduleInformation);
            }
            ++it;
        }
    }
}

QSet<QString> QnGlobalModuleFinder::getModuleAddresses(const QnUuid &id) const {
    QSet<QString> result;

    for (const QSet<QString> &addresses: m_discoveredAddresses.value(id))
        result.unite(addresses);

    return result;
}
