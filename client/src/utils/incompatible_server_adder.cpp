#include "incompatible_server_adder.h"

#include <api/app_server_connection.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <utils/network/global_module_finder.h>
#include <common/common_module.h>

namespace {

void updateServer(const QnMediaServerResourcePtr &server, const QnModuleInformation &moduleInformation) {
    QList<QHostAddress> addressList;
    foreach (const QString &address, moduleInformation.remoteAddresses)
        addressList.append(QHostAddress(address));
    server->setNetAddrList(addressList);

    if (!addressList.isEmpty()) {
        QString url = QString(lit("http://%1:%2")).arg(addressList.first().toString()).arg(moduleInformation.port);
        server->setApiUrl(url);
        server->setUrl(url);
    }
    server->setVersion(moduleInformation.version);
    server->setSystemInfo(moduleInformation.systemInformation);
    server->setSystemName(moduleInformation.systemName);
}

QnMediaServerResourcePtr makeResource(const QnModuleInformation &moduleInformation) {
    QnMediaServerResourcePtr server(new QnMediaServerResource(qnResTypePool));

    server->setId(QUuid::createUuid());
    server->setStatus(Qn::Incompatible, true);
    server->setProperty(lit("guid"), moduleInformation.id.toString());

    updateServer(server, moduleInformation);

    return server;
}

bool isSuitable(const QnModuleInformation &moduleInformation) {
    return moduleInformation.version >= QnSoftwareVersion(2, 3, 0, 0);
}

} // anonymous namespace

QnIncompatibleServerAdder::QnIncompatibleServerAdder(QObject *parent) :
    QObject(parent)
{
    connect(QnGlobalModuleFinder::instance(),   &QnGlobalModuleFinder::peerChanged, this,   &QnIncompatibleServerAdder::at_peerChanged);
    connect(QnGlobalModuleFinder::instance(),   &QnGlobalModuleFinder::peerLost,    this,   &QnIncompatibleServerAdder::at_peerLost);
    connect(qnResPool,                          &QnResourcePool::resourceAdded,     this,   &QnIncompatibleServerAdder::at_reourcePool_resourceChanged);
    connect(qnResPool,                          &QnResourcePool::resourceChanged,   this,   &QnIncompatibleServerAdder::at_reourcePool_resourceChanged);

    // fill resource pool with already found modules
    foreach (const QnModuleInformation &moduleInformation, QnGlobalModuleFinder::instance()->foundModules())
        at_peerChanged(moduleInformation);
}

void QnIncompatibleServerAdder::at_peerChanged(const QnModuleInformation &moduleInformation) {
    if (moduleInformation.isCompatibleToCurrentSystem()) {
        removeResource(moduleInformation.id);
        return;
    }

    if (moduleInformation.systemName == qnCommon->localSystemName()) {
        QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(moduleInformation.id, true).dynamicCast<QnMediaServerResource>();
        if (server) {
            updateServer(server, moduleInformation);
            server->setStatus(Qn::Incompatible);
            return;
        }
    }

    QUuid id = m_fakeUuidByServerUuid[moduleInformation.id];
    if (id.isNull()) {
        // add a resource
        if (!isSuitable(moduleInformation))
            return;

        QnMediaServerResourcePtr server = makeResource(moduleInformation);
        m_fakeUuidByServerUuid[moduleInformation.id] = server->getId();
        m_serverUuidByFakeUuid[server->getId()] = moduleInformation.id;
        qnResPool->addResource(server);
    } else {
        // update the resource
        QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(id).dynamicCast<QnMediaServerResource>();
        Q_ASSERT_X(server, "There must be a resource in the resource pool.", Q_FUNC_INFO);
        updateServer(server, moduleInformation);
    }
}

void QnIncompatibleServerAdder::at_peerLost(const QnModuleInformation &moduleInformation) {
    removeResource(moduleInformation.id);
}

void QnIncompatibleServerAdder::at_reourcePool_resourceChanged(const QnResourcePtr &resource) {
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    Qn::ResourceStatus status = server->getStatus();
    if (status == Qn::Online)
        removeResource(m_fakeUuidByServerUuid[server->getId()]);
}

void QnIncompatibleServerAdder::removeResource(const QUuid &id) {
    if (id.isNull())
        return;

    QUuid serverId = m_serverUuidByFakeUuid.take(id);
    if (serverId.isNull())
        return;

    m_fakeUuidByServerUuid.remove(serverId);

    QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(id).dynamicCast<QnMediaServerResource>();
    if (server)
        qnResPool->removeResource(server);
}
