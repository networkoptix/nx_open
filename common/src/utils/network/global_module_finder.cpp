#include "global_module_finder.h"

#include <core/resource_management/resource_pool.h>
#include <utils/network/module_finder.h>
#include <utils/network/router.h>
#include <utils/common/log.h>
#include <common/common_module.h>
#include <api/app_server_connection.h>
#include <nx_ec/data/api_module_data.h>
#include <nx_ec/dummy_handler.h>
#include <nx_ec/ec_proto_version.h>

QnGlobalModuleFinder::QnGlobalModuleFinder(QnModuleFinder *moduleFinder, QObject *parent) :
    QObject(parent),
    m_mutex(QnMutex::Recursive),
    m_connection(std::weak_ptr<ec2::AbstractECConnection>()),
    m_moduleFinder(moduleFinder)
{
    connect(QnRouter::instance(),   &QnRouter::connectionAdded,     this,       &QnGlobalModuleFinder::at_router_connectionAdded);
    connect(QnRouter::instance(),   &QnRouter::connectionRemoved,   this,       &QnGlobalModuleFinder::at_router_connectionRemoved);

    QMultiHash<QnUuid, QnRouter::Endpoint> connections = QnRouter::instance()->connections();
    for (auto it = connections.begin(); it != connections.end(); ++it) {
		/* Ignore addresses discovered by client */
		if (!moduleFinder && it.key() == qnCommon->moduleGUID())
			continue;

        at_router_connectionAdded(it.key(), it->id, it->host);
	}

    if (moduleFinder) {
        for (const QnModuleInformation &moduleInformation: moduleFinder->foundModules())
            addModule(moduleInformation);

        connect(moduleFinder,               &QnModuleFinder::moduleChanged, this,   &QnGlobalModuleFinder::at_moduleFinder_moduleChanged);
        connect(moduleFinder,               &QnModuleFinder::moduleLost,    this,   &QnGlobalModuleFinder::at_moduleFinder_moduleLost);
    }
}

void QnGlobalModuleFinder::setConnection(const ec2::AbstractECConnectionPtr &connection) {
    SCOPED_MUTEX_LOCK( lock, &m_mutex);

    ec2::AbstractECConnectionPtr oldConnection = m_connection.lock();
    QList<QnModuleInformation> foundModules = m_moduleInformationById.values();
    m_moduleInformationById.clear();
    m_connection = connection;

    lock.unlock();

    if (oldConnection)
        oldConnection->getMiscManager()->disconnect(this);

    for (const QnModuleInformation &moduleInformation: foundModules)
        emit peerLost(moduleInformation);

    if (connection)
        connect(connection->getMiscManager().get(), &ec2::AbstractMiscManager::moduleChanged, this, &QnGlobalModuleFinder::at_moduleChanged, Qt::QueuedConnection);

    if (m_moduleFinder) {
        for (const QnModuleInformation &moduleInformation: m_moduleFinder->foundModules())
            addModule(moduleInformation);
    }
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
    data->protoVersion = moduleInformation.protoVersion;
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
    moduleInformation->protoVersion = data.protoVersion == 0 ? nx_ec::INITIAL_EC2_PROTO_VERSION : data.protoVersion;
}

QList<QnModuleInformation> QnGlobalModuleFinder::foundModules() const {
    SCOPED_MUTEX_LOCK( lock, &m_mutex);

    QList<QnModuleInformation> result;
    for (const QnModuleInformation &moduleInformation: m_moduleInformationById) {
        if (!moduleInformation.remoteAddresses.isEmpty())
            result.append(moduleInformation);
    }
    return result;
}

QnModuleInformation QnGlobalModuleFinder::moduleInformation(const QnUuid &id) const {
    SCOPED_MUTEX_LOCK( lock, &m_mutex);
    return m_moduleInformationById[id];
}

void QnGlobalModuleFinder::at_moduleChanged(const QnModuleInformation &moduleInformation, bool isAlive) {
    if (moduleInformation.id == qnCommon->moduleGUID())
        return;

    if (isAlive)
        addModule(moduleInformation);
}

void QnGlobalModuleFinder::at_moduleFinder_moduleChanged(const QnModuleInformation &moduleInformation) {
    addModule(moduleInformation);

    SCOPED_MUTEX_LOCK( lock, &m_mutex);
    ec2::AbstractECConnectionPtr connection = m_connection.lock();
    lock.unlock();

    if (connection)
        connection->getMiscManager()->sendModuleInformation(moduleInformation, true, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
}

void QnGlobalModuleFinder::at_moduleFinder_moduleLost(const QnModuleInformation &moduleInformation) {
    SCOPED_MUTEX_LOCK( lock, &m_mutex);
    ec2::AbstractECConnectionPtr connection = m_connection.lock();
    lock.unlock();

    if (connection)
        connection->getMiscManager()->sendModuleInformation(moduleInformation, false, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
}

void QnGlobalModuleFinder::at_router_connectionAdded(const QnUuid &discovererId, const QnUuid &peerId, const QString &host) {
	/* Ignore addresses discovered by client */
	if (!m_moduleFinder && discovererId == qnCommon->moduleGUID())
		return;

    {
        SCOPED_MUTEX_LOCK( lock, &m_mutex);

        QSet<QString> &addresses = m_discoveredAddresses[peerId][discovererId];
        auto it = addresses.find(host);
        if (it != addresses.end())
            return;
        addresses.insert(host);
    }
    updateAddresses(peerId);
}

void QnGlobalModuleFinder::at_router_connectionRemoved(const QnUuid &discovererId, const QnUuid &peerId, const QString &host) {
	/* Ignore addresses discovered by client */
	if (!m_moduleFinder && discovererId == qnCommon->moduleGUID())
		return;

    {
        SCOPED_MUTEX_LOCK( lock, &m_mutex);

        if (!m_discoveredAddresses[peerId][discovererId].remove(host))
            return;
    }
    updateAddresses(peerId);
}

void QnGlobalModuleFinder::updateAddresses(const QnUuid &id) {
    SCOPED_MUTEX_LOCK( lock, &m_mutex);

    QnModuleInformation moduleInformation = m_moduleInformationById.value(id);
    if (moduleInformation.id.isNull())
        return;

    QSet<QString> addresses = getModuleAddresses(id);
    if (moduleInformation.remoteAddresses == addresses)
        return;

    moduleInformation.remoteAddresses = addresses;
    m_moduleInformationById[id] = moduleInformation;

    lock.unlock();

    if (moduleInformation.remoteAddresses.isEmpty()) {
        NX_LOG(lit("QnGlobalModuleFinder. Module %1 is lost").arg(moduleInformation.id.toString()), cl_logDEBUG1);
        emit peerLost(moduleInformation);
    } else {
        NX_LOG(lit("QnGlobalModuleFinder. Module %1 is changed, addresses = [%2]")
               .arg(moduleInformation.id.toString())
               .arg(QStringList(QStringList::fromSet(moduleInformation.remoteAddresses)).join(lit(", "))), cl_logDEBUG1);
        emit peerChanged(moduleInformation);
    }
}

void QnGlobalModuleFinder::addModule(const QnModuleInformation &moduleInformation) {
    if (moduleInformation.id == qnCommon->moduleGUID())
        return;

    SCOPED_MUTEX_LOCK( lock, &m_mutex);

    QnModuleInformation updatedModuleInformation = moduleInformation;
    updatedModuleInformation.remoteAddresses = getModuleAddresses(moduleInformation.id);
    if (updatedModuleInformation.remoteAddresses.isEmpty()) {
        m_moduleInformationById[moduleInformation.id] = updatedModuleInformation;
        return;
    }

    QnModuleInformation &oldModuleInformation = m_moduleInformationById[moduleInformation.id];
    if (oldModuleInformation != updatedModuleInformation) {
        oldModuleInformation = updatedModuleInformation;

        lock.unlock();

        NX_LOG(lit("QnGlobalModuleFinder. Module %1 is changed, addresses = [%2]")
               .arg(updatedModuleInformation.id.toString())
               .arg(QStringList(QStringList::fromSet(updatedModuleInformation.remoteAddresses)).join(lit(", "))), cl_logDEBUG1);
        emit peerChanged(moduleInformation);
    }
}

QSet<QString> QnGlobalModuleFinder::getModuleAddresses(const QnUuid &id) const {
    QSet<QString> result;

    for (const QSet<QString> &addresses: m_discoveredAddresses.value(id))
        result.unite(addresses);

    return result;
}
