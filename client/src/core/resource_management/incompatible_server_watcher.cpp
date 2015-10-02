#include "incompatible_server_watcher.h"

#include <api/common_message_processor.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <utils/common/log.h>
#include <common/common_module.h>

namespace {

void updateServer(const QnMediaServerResourcePtr &server, const QnModuleInformationWithAddresses &moduleInformation) {
    QList<QHostAddress> addressList;
    foreach (const QString &address, moduleInformation.remoteAddresses)
        addressList.append(QHostAddress(address));
    server->setNetAddrList(addressList);

    if (!addressList.isEmpty()) {
        QString address = addressList.first().toString();
        quint16 port = moduleInformation.port;
        QString url = QString(lit("http://%1:%2")).arg(address).arg(port);
        server->setApiUrl(url);
        server->setUrl(url);
    }
    if (!moduleInformation.name.isEmpty())
        server->setName(moduleInformation.name);
    server->setVersion(moduleInformation.version);
    server->setSystemInfo(moduleInformation.systemInformation);
    server->setSystemName(moduleInformation.systemName);
    server->setProperty(lit("protoVersion"), QString::number(moduleInformation.protoVersion));
}

QnMediaServerResourcePtr makeResource(const QnModuleInformationWithAddresses &moduleInformation, Qn::ResourceStatus initialStatus) {
    QnMediaServerResourcePtr server(new QnMediaServerResource(qnResTypePool));

    server->setId(QnUuid::createUuid());
    server->setStatus(initialStatus, true);
    server->setOriginalGuid(moduleInformation.id);

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
}

QnIncompatibleServerWatcher::~QnIncompatibleServerWatcher() {
    stop();
}

void QnIncompatibleServerWatcher::start() {
    connect(QnCommonMessageProcessor::instance(), &QnCommonMessageProcessor::moduleChanged, this, &QnIncompatibleServerWatcher::at_moduleChanged);
    connect(qnResPool,  &QnResourcePool::resourceAdded,     this,   &QnIncompatibleServerWatcher::at_resourcePool_resourceChanged);
    connect(qnResPool,  &QnResourcePool::resourceChanged,   this,   &QnIncompatibleServerWatcher::at_resourcePool_resourceChanged);
    connect(qnResPool,  &QnResourcePool::statusChanged,     this,   &QnIncompatibleServerWatcher::at_resourcePool_resourceChanged);
}

void QnIncompatibleServerWatcher::stop() {
    if (QnCommonMessageProcessor::instance())
        disconnect(QnCommonMessageProcessor::instance(), &QnCommonMessageProcessor::moduleChanged, this, &QnIncompatibleServerWatcher::at_moduleChanged);
    disconnect(qnResPool, 0, this, 0);

    QList<QnUuid> ids;
    {
        QnMutexLocker lock( &m_mutex );
        ids = m_fakeUuidByServerUuid.values();
        m_fakeUuidByServerUuid.clear();
        m_serverUuidByFakeUuid.clear();
    }

    for (const QnUuid &id: ids) {
        QnResourcePtr resource = qnResPool->getIncompatibleResourceById(id, true);
        if (resource)
            qnResPool->removeResource(resource);
    }
}

void QnIncompatibleServerWatcher::keepServer(const QnUuid &id, bool keep) {
    QnMutexLocker lock(&m_mutex);

    auto it = m_moduleInformationById.find(id);
    if (it == m_moduleInformationById.end())
        return;

    it->keep = keep;

    if (!it->removed)
        return;

    m_moduleInformationById.erase(it);

    lock.unlock();

    removeResource(getFakeId(id));
}

void QnIncompatibleServerWatcher::createModules(const QList<QnModuleInformationWithAddresses> &modules) {
    for (const QnModuleInformationWithAddresses &moduleInformation: modules)
        at_moduleChanged(moduleInformation, true);
}

void QnIncompatibleServerWatcher::at_resourcePool_resourceChanged(const QnResourcePtr &resource) {
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    QnUuid id = server->getId();

    {
        QnMutexLocker lock( &m_mutex );
        if (m_serverUuidByFakeUuid.contains(id))
            return;
    }

    Qn::ResourceStatus status = server->getStatus();
    if (status != Qn::Offline && server->getModuleInformation().isCompatibleToCurrentSystem()) {
        removeResource(getFakeId(id));
    } else if (status == Qn::Offline) {
        QnMutexLocker lock( &m_mutex );
        QnModuleInformationWithAddresses moduleInformation = m_moduleInformationById.value(id).moduleInformation;
        lock.unlock();
        if (!moduleInformation.id.isNull())
            addResource(moduleInformation);
    }
}

void QnIncompatibleServerWatcher::at_moduleChanged(const QnModuleInformationWithAddresses &moduleInformation, bool isAlive) {
    QnMutexLocker lock(&m_mutex);
    auto it = m_moduleInformationById.find(moduleInformation.id);

    if (!isAlive) {
        if (it == m_moduleInformationById.end())
            return;

        if (it->keep) {
            it->removed = true;
            return;
        }

        m_moduleInformationById.remove(moduleInformation.id);

        lock.unlock();
        removeResource(getFakeId(moduleInformation.id));
    } else {
        if (it != m_moduleInformationById.end()) {
            it->removed = false;
            it->moduleInformation = moduleInformation;
        } else {
            m_moduleInformationById.insert(moduleInformation.id, moduleInformation);
        }

        lock.unlock();
        addResource(moduleInformation);
    }
}

void QnIncompatibleServerWatcher::addResource(const QnModuleInformationWithAddresses &moduleInformation) {
    bool compatible = moduleInformation.isCompatibleToCurrentSystem();
    bool authorized = moduleInformation.authHash == qnCommon->moduleInformation().authHash;

    QnUuid id = getFakeId(moduleInformation.id);

    QnResourcePtr resource = qnResPool->getResourceById(moduleInformation.id);
    if ((compatible && (authorized || resource)) || (resource && resource->getStatus() == Qn::Online)) {
        removeResource(id);
        return;
    }

    if (id.isNull()) {
        // add a resource
        if (!isSuitable(moduleInformation))
            return;

        QnMediaServerResourcePtr server = makeResource(moduleInformation, (compatible && !authorized) ? Qn::Unauthorized : Qn::Incompatible);
        {
            QnMutexLocker lock( &m_mutex );
            m_fakeUuidByServerUuid[moduleInformation.id] = server->getId();
            m_serverUuidByFakeUuid[server->getId()] = moduleInformation.id;
        }
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
        if (!server)
            return;
        updateServer(server, moduleInformation);

        NX_LOG(lit("QnIncompatibleServerWatcher: Update incompatible server %1 at %2 [%3]")
            .arg(moduleInformation.id.toString())
            .arg(moduleInformation.systemName)
            .arg(QStringList(moduleInformation.remoteAddresses.toList()).join(lit(", "))),
            cl_logDEBUG1);
    }
}

void QnIncompatibleServerWatcher::removeResource(const QnUuid &id) {
    if (id.isNull())
        return;

    QnUuid serverId;
    {
        QnMutexLocker lock( &m_mutex );
        serverId = m_serverUuidByFakeUuid.take(id);
        if (serverId.isNull())
            return;

        m_fakeUuidByServerUuid.remove(serverId);
    }

    QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(id, true).dynamicCast<QnMediaServerResource>();
    if (server) {
        NX_LOG(lit("QnIncompatibleServerWatcher: Remove incompatible server %1 at %2")
            .arg(serverId.toString())
            .arg(server->getSystemName()),
            cl_logDEBUG1);

        qnResPool->removeResource(server);
    }
}

QnUuid QnIncompatibleServerWatcher::getFakeId(const QnUuid &realId) const {
    QnMutexLocker lock( &m_mutex );
    return m_fakeUuidByServerUuid.value(realId);
}
