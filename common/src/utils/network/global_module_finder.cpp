#include "global_module_finder.h"

#include <core/resource_management/resource_pool.h>
#include <utils/network/module_finder.h>
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
        foreach (const QnModuleInformation &moduleInformation, moduleFinder->foundModules())
            addModule(moduleInformation, qnCommon->moduleGUID());

        connect(moduleFinder,               &QnModuleFinder::moduleChanged, this,   &QnGlobalModuleFinder::at_moduleFinder_moduleChanged);
        connect(moduleFinder,               &QnModuleFinder::moduleLost,    this,   &QnGlobalModuleFinder::at_moduleFinder_moduleLost);
    }
}

void QnGlobalModuleFinder::setConnection(const ec2::AbstractECConnectionPtr &connection) {
    ec2::AbstractECConnectionPtr oldConnection = m_connection.lock();

    if (oldConnection)
        oldConnection->getMiscManager()->disconnect(this);

    m_connection = connection;

    if (connection)
        connect(connection->getMiscManager().get(),        &ec2::AbstractMiscManager::moduleChanged,  this,   &QnGlobalModuleFinder::at_moduleChanged);
}

void QnGlobalModuleFinder::fillApiModuleData(const QnModuleInformation &moduleInformation, ec2::ApiModuleData *data) {
    data->type = moduleInformation.type;
    data->id = moduleInformation.id;
    data->systemName = moduleInformation.systemName;
    data->version = moduleInformation.version.toString();
    data->systemInformation = moduleInformation.systemInformation.toString();
    data->addresses = moduleInformation.remoteAddresses.toList();
    data->port = moduleInformation.port;
    data->isAlive = true;
}

void QnGlobalModuleFinder::fillFromApiModuleData(const ec2::ApiModuleData &data, QnModuleInformation *moduleInformation) {
    moduleInformation->type = data.type;
    moduleInformation->id = data.id;
    moduleInformation->systemName = data.systemName;
    moduleInformation->version = QnSoftwareVersion(data.version);
    moduleInformation->systemInformation = QnSystemInformation(data.systemInformation);
    moduleInformation->remoteAddresses = QSet<QString>::fromList(data.addresses);
    moduleInformation->port = data.port;
}

QList<QnModuleInformation> QnGlobalModuleFinder::foundModules() const {
    return m_moduleInformationById.values();
}

QSet<QUuid> QnGlobalModuleFinder::discoverers(const QUuid &moduleId) {
    return m_discovererIdByServerId.value(moduleId);
}

QnModuleInformation QnGlobalModuleFinder::moduleInformation(const QUuid &id) const {
    return m_moduleInformationById[id];
}

void QnGlobalModuleFinder::at_moduleChanged(const QnModuleInformation &moduleInformation, bool isAlive, const QUuid &discoverer) {
    if (isAlive)
        addModule(moduleInformation, discoverer);
    else
        removeModule(moduleInformation, discoverer);
}

void QnGlobalModuleFinder::at_moduleFinder_moduleChanged(const QnModuleInformation &moduleInformation) {
    addModule(moduleInformation, QUuid(qnCommon->moduleGUID()));
    if (ec2::AbstractECConnectionPtr connection = m_connection.lock())
        connection->getMiscManager()->sendModuleInformation(moduleInformation, true, QUuid(qnCommon->moduleGUID()), ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
}

void QnGlobalModuleFinder::at_moduleFinder_moduleLost(const QnModuleInformation &moduleInformation) {
    removeModule(moduleInformation, QUuid(qnCommon->moduleGUID()));
    if (ec2::AbstractECConnectionPtr connection = m_connection.lock())
        connection->getMiscManager()->sendModuleInformation(moduleInformation, false, QUuid(qnCommon->moduleGUID()), ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
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

void QnGlobalModuleFinder::addModule(const QnModuleInformation &moduleInformation, const QUuid &discoverer) {
    if (moduleInformation.id == qnCommon->moduleGUID())
        return;

    m_discoveredAddresses[moduleInformation.id][discoverer] = moduleInformation.remoteAddresses;

    QnModuleInformation updatedModuleInformation = moduleInformation;
    updatedModuleInformation.remoteAddresses = getModuleAddresses(moduleInformation.id);

    m_discovererIdByServerId[moduleInformation.id].insert(discoverer);

    QnModuleInformation &oldModuleInformation = m_moduleInformationById[moduleInformation.id];
    if (oldModuleInformation != updatedModuleInformation) {
        oldModuleInformation = updatedModuleInformation;
        emit peerChanged(moduleInformation);
    }
}

void QnGlobalModuleFinder::removeModule(const QnModuleInformation &moduleInformation, const QUuid &discoverer) {
    if (moduleInformation.id == qnCommon->moduleGUID())
        return;

    m_discoveredAddresses[moduleInformation.id].remove(discoverer);

    QnModuleInformation updatedModuleInformation = moduleInformation;
    updatedModuleInformation.remoteAddresses = getModuleAddresses(moduleInformation.id);

    m_discovererIdByServerId[moduleInformation.id].remove(discoverer);

    if (updatedModuleInformation.remoteAddresses.isEmpty()) {
        m_moduleInformationById.remove(updatedModuleInformation.id);
        emit peerLost(updatedModuleInformation);
    } else {
        m_moduleInformationById[moduleInformation.id] = updatedModuleInformation;
        emit peerChanged(updatedModuleInformation);
    }
}

void QnGlobalModuleFinder::removeAllModulesDiscoveredBy(const QUuid &discoverer) {
    if (discoverer == qnCommon->moduleGUID()) {
        qWarning() << "Trying to remove our own modules";
        return;
    }

    for (auto it = m_moduleInformationById.begin(); it != m_moduleInformationById.end(); /* no inc */) {
        QSet<QUuid> &discoverers = m_discovererIdByServerId[it.key()];
        if (discoverers.remove(discoverer) ) {
            if (discoverers.isEmpty()) {
                emit peerLost(it.value());
                it = m_moduleInformationById.erase(it);
                continue;
            }
        }
        ++it;
    }

    for (auto it = m_discoveredAddresses.begin(); it != m_discoveredAddresses.end(); ++it)
        it.value().remove(discoverer);
}

QSet<QString> QnGlobalModuleFinder::getModuleAddresses(const QUuid &id) const {
    QSet<QString> result;

    foreach (const QSet<QString> &addresses, m_discoveredAddresses.value(id))
        result.unite(addresses);

    return result;
}
