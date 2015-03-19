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
    m_mutex(QMutex::Recursive),
    m_connection(std::weak_ptr<ec2::AbstractECConnection>()),
    m_moduleFinder(moduleFinder)
{
    if (moduleFinder) {
        for (const QnModuleInformation &moduleInformation: moduleFinder->foundModules())
            addModule(moduleInformation);

        connect(moduleFinder,               &QnModuleFinder::moduleChanged, this,   &QnGlobalModuleFinder::at_moduleFinder_moduleChanged);
        connect(moduleFinder,               &QnModuleFinder::moduleLost,    this,   &QnGlobalModuleFinder::at_moduleFinder_moduleLost);
    }
}

void QnGlobalModuleFinder::setConnection(const ec2::AbstractECConnectionPtr &connection) {
    QMutexLocker lock(&m_mutex);

    ec2::AbstractECConnectionPtr oldConnection = m_connection.lock();
    QList<QnGlobalModuleInformation> foundModules = m_moduleInformationById.values();
    m_moduleInformationById.clear();
    m_connection = connection;

    lock.unlock();

    if (oldConnection)
        oldConnection->getMiscManager()->disconnect(this);

    for (const QnGlobalModuleInformation &moduleInformation: foundModules)
        emit peerLost(moduleInformation);

    if (connection)
        connect(connection->getMiscManager().get(), &ec2::AbstractMiscManager::moduleChanged, this, &QnGlobalModuleFinder::at_moduleChanged, Qt::QueuedConnection);

    if (m_moduleFinder) {
        for (const QnModuleInformation &moduleInformation: m_moduleFinder->foundModules())
            addModule(moduleInformation);
    }
}

QList<QnGlobalModuleInformation> QnGlobalModuleFinder::foundModules() const {
    QMutexLocker lock(&m_mutex);

    QList<QnGlobalModuleInformation> result;
    for (const QnGlobalModuleInformation &moduleInformation: m_moduleInformationById) {
        if (!moduleInformation.remoteAddresses.isEmpty())
            result.append(moduleInformation);
    }
    return result;
}

QnGlobalModuleInformation QnGlobalModuleFinder::moduleInformation(const QnUuid &id) const {
    QMutexLocker lock(&m_mutex);
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

    QMutexLocker lock(&m_mutex);
    ec2::AbstractECConnectionPtr connection = m_connection.lock();
    lock.unlock();

    if (connection)
        connection->getMiscManager()->sendModuleInformation(moduleInformation, true, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
}

void QnGlobalModuleFinder::at_moduleFinder_moduleLost(const QnModuleInformation &moduleInformation) {
    QMutexLocker lock(&m_mutex);
    ec2::AbstractECConnectionPtr connection = m_connection.lock();
    lock.unlock();

    if (connection)
        connection->getMiscManager()->sendModuleInformation(moduleInformation, false, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
}

void QnGlobalModuleFinder::updateAddresses(const QnUuid &id) {
    QMutexLocker lock(&m_mutex);

    QnGlobalModuleInformation moduleInformation = m_moduleInformationById.value(id);
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

    QMutexLocker lock(&m_mutex);

    QnGlobalModuleInformation updatedModuleInformation = moduleInformation;
    updatedModuleInformation.remoteAddresses = getModuleAddresses(moduleInformation.id);
    if (updatedModuleInformation.remoteAddresses.isEmpty()) {
        m_moduleInformationById[moduleInformation.id] = updatedModuleInformation;
        return;
    }

    QnGlobalModuleInformation &oldModuleInformation = m_moduleInformationById[moduleInformation.id];
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
