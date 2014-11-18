#include "incompatible_server_watcher.h"

#include <api/app_server_connection.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <utils/network/global_module_finder.h>
#include <utils/network/router.h>
#include <utils/common/log.h>
#include <common/common_module.h>

namespace {

void updateServer(const QnMediaServerResourcePtr &server, const QnModuleInformation &moduleInformation) {
    QList<QHostAddress> addressList;
    foreach (const QString &address, moduleInformation.remoteAddresses)
        addressList.append(QHostAddress(address));
    server->setNetAddrList(addressList);

    if (!addressList.isEmpty()) {
        QString address = addressList.first().toString();
        quint16 port = moduleInformation.port;
        if (QnRouter::instance()) {
            QnRoute route = QnRouter::instance()->routeTo(moduleInformation.id, qnCommon->remoteGUID());
            if (route.isValid()) {
                address = route.points.last().host;
                port = route.points.last().port;
            }
        }
        QString url = QString(lit("http://%1:%2")).arg(address).arg(port);
        server->setApiUrl(url);
        server->setUrl(url);
    }
    if (!moduleInformation.name.isEmpty())
        server->setName(moduleInformation.name);
    server->setVersion(moduleInformation.version);
    server->setSystemInfo(moduleInformation.systemInformation);
    server->setSystemName(moduleInformation.systemName);
}

QnMediaServerResourcePtr makeResource(const QnModuleInformation &moduleInformation, Qn::ResourceStatus initialStatus) {
    QnMediaServerResourcePtr server(new QnMediaServerResource(qnResTypePool));

    server->setId(QnUuid::createUuid());
    server->setStatus(initialStatus, true);
    server->setProperty(lit("guid"), moduleInformation.id.toString());

    updateServer(server, moduleInformation);

    return server;
}

bool isSuitable(const QnModuleInformation &moduleInformation) {
    return moduleInformation.version >= QnSoftwareVersion(2, 3, 0, 0);
}

} // anonymous namespace

QnIncompatibleServerWatcher::QnIncompatibleServerWatcher(QObject *parent) :
    QObject(parent)
{
    connect(QnGlobalModuleFinder::instance(),   &QnGlobalModuleFinder::peerChanged, this,   &QnIncompatibleServerWatcher::at_peerChanged);
    connect(QnGlobalModuleFinder::instance(),   &QnGlobalModuleFinder::peerLost,    this,   &QnIncompatibleServerWatcher::at_peerLost);
    connect(qnResPool,                          &QnResourcePool::resourceAdded,     this,   &QnIncompatibleServerWatcher::at_resourcePool_resourceChanged);
    connect(qnResPool,                          &QnResourcePool::resourceChanged,   this,   &QnIncompatibleServerWatcher::at_resourcePool_resourceChanged);

    // fill resource pool with already found modules
    foreach (const QnModuleInformation &moduleInformation, QnGlobalModuleFinder::instance()->foundModules())
        at_peerChanged(moduleInformation);
}

QnIncompatibleServerWatcher::~QnIncompatibleServerWatcher() {
    foreach (const QnUuid &id, m_fakeUuidByServerUuid) {
        QnResourcePtr resource = qnResPool->getIncompatibleResourceById(id, true);
        if (resource)
            qnResPool->removeResource(resource);
    }
}

void QnIncompatibleServerWatcher::at_peerChanged(const QnModuleInformation &moduleInformation) {
    bool compatible = moduleInformation.isCompatibleToCurrentSystem();
    bool authorized = moduleInformation.authHash == qnCommon->moduleInformation().authHash;

    QnResourcePtr resource = qnResPool->getResourceById(moduleInformation.id);
    if ((compatible && (authorized || resource)) || (resource && resource->getStatus() == Qn::Online)) {
        removeResource(m_fakeUuidByServerUuid.value(moduleInformation.id));
        return;
    }

    QnUuid id = m_fakeUuidByServerUuid.value(moduleInformation.id);
    if (id.isNull()) {
        // add a resource
        if (!isSuitable(moduleInformation))
            return;

        QnMediaServerResourcePtr server = makeResource(moduleInformation, (compatible && !authorized) ? Qn::Unauthorized : Qn::Incompatible);
        m_fakeUuidByServerUuid[moduleInformation.id] = server->getId();
        m_serverUuidByFakeUuid[server->getId()] = moduleInformation.id;
        qnResPool->addResource(server);

		NX_LOG(lit("QnIncompatibleServerWatcher: Add incompatible server %1 at %2 [%3]")
			.arg(moduleInformation.id.toString())
			.arg(moduleInformation.systemName)
			.arg(QStringList(moduleInformation.remoteAddresses.toList()).join(lit(", "))),
			cl_logDEBUG1);
    } else {
        // update the resource
        QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(id, true).dynamicCast<QnMediaServerResource>();
        Q_ASSERT_X(server, "There must be a resource in the resource pool.", Q_FUNC_INFO);
        updateServer(server, moduleInformation);

		NX_LOG(lit("QnIncompatibleServerWatcher: Update incompatible server %1 at %2 [%3]")
			.arg(moduleInformation.id.toString())
			.arg(moduleInformation.systemName)
			.arg(QStringList(moduleInformation.remoteAddresses.toList()).join(lit(", "))),
			cl_logDEBUG1);
    }
}

void QnIncompatibleServerWatcher::at_peerLost(const QnModuleInformation &moduleInformation) {
    removeResource(m_fakeUuidByServerUuid.value(moduleInformation.id));
}

void QnIncompatibleServerWatcher::at_resourcePool_resourceChanged(const QnResourcePtr &resource) {
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    QnUuid id = server->getId();

    if (m_serverUuidByFakeUuid.contains(id))
        return;

    Qn::ResourceStatus status = server->getStatus();
    if (status != Qn::Offline && server->getModuleInformation().isCompatibleToCurrentSystem())
        removeResource(m_fakeUuidByServerUuid.value(id));
}

void QnIncompatibleServerWatcher::removeResource(const QnUuid &id) {
    if (id.isNull())
        return;

    QnUuid serverId = m_serverUuidByFakeUuid.take(id);
    if (serverId.isNull())
        return;

    m_fakeUuidByServerUuid.remove(serverId);

    QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(id, true).dynamicCast<QnMediaServerResource>();
    if (server)
        qnResPool->removeResource(server);
}
