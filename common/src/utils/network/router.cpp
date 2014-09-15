#include "router.h"

#include "utils/common/log.h"
#include "nx_ec/dummy_handler.h"
#include "nx_ec/ec_api.h"
#include "common/common_module.h"
#include "route_builder.h"
#include "module_finder.h"

QnRouter::QnRouter(QnModuleFinder *moduleFinder, bool passive, QObject *parent) :
    QObject(parent),
    m_mutex(QMutex::Recursive),
    m_connection(std::weak_ptr<ec2::AbstractECConnection>()),
    m_routeBuilder(new QnRouteBuilder(qnCommon->moduleGUID())),
    m_passive(passive)
{
    connect(moduleFinder,       &QnModuleFinder::moduleUrlFound,    this,   &QnRouter::at_moduleFinder_moduleUrlFound);
    connect(moduleFinder,       &QnModuleFinder::moduleUrlLost,     this,   &QnRouter::at_moduleFinder_moduleUrlLost);
}

QnRouter::~QnRouter() {}

QMultiHash<QUuid, QnRouter::Endpoint> QnRouter::connections() const {
    QMutexLocker lk(&m_mutex);
    return m_connections;
}

QHash<QUuid, QnRouteList> QnRouter::routes() const {
    QMutexLocker lk(&m_mutex);
    return m_routeBuilder->routes();
}

QnRoute QnRouter::routeTo(const QUuid &id) const {
    QMutexLocker lk(&m_mutex);
    return m_routeBuilder->routeTo(id);
}

QnRoute QnRouter::routeTo(const QString &host, quint16 port) const {
    QUuid id = whoIs(host, port);
    return id.isNull() ? QnRoute() : routeTo(id);
}

QUuid QnRouter::whoIs(const QString &host, quint16 port) const {
    QMutexLocker lk(&m_mutex);

    for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
        if (it->host == host && it->port == port)
            return it->id;
    }
    return QUuid();
}

void QnRouter::at_moduleFinder_moduleUrlFound(const QnModuleInformation &moduleInformation, const QUrl &url) {
    Endpoint endpoint(moduleInformation.id, url.host(), url.port());

    if (!addConnection(qnCommon->moduleGUID(), endpoint))
        return;

    if (!m_passive) {
        QnRuntimeInfoManager *runtimeInfoManager = QnRuntimeInfoManager::instance();
        if (runtimeInfoManager) {
            QnPeerRuntimeInfo localInfo = runtimeInfoManager->localInfo();
            ec2::ApiConnectionData connection;
            connection.peerId = endpoint.id;
            connection.host = endpoint.host;
            connection.port = endpoint.port;
            localInfo.data.availableConnections.append(connection);
            runtimeInfoManager->updateLocalItem(localInfo);
        }
    }

    emit connectionAdded(qnCommon->moduleGUID(), endpoint.id, endpoint.host, endpoint.port);
}

void QnRouter::at_moduleFinder_moduleUrlLost(const QnModuleInformation &moduleInformation, const QUrl &url) {
    Endpoint endpoint(moduleInformation.id, url.host(), url.port());

    if (!removeConnection(qnCommon->moduleGUID(), endpoint))
        return;

    if (!m_passive) {
        QnRuntimeInfoManager *runtimeInfoManager = QnRuntimeInfoManager::instance();
        if (runtimeInfoManager) {
            QnPeerRuntimeInfo localInfo = runtimeInfoManager->localInfo();
            ec2::ApiConnectionData connection;
            connection.peerId = endpoint.id;
            connection.host = endpoint.host;
            connection.port = endpoint.port;
            localInfo.data.availableConnections.removeOne(connection);
            runtimeInfoManager->updateLocalItem(localInfo);
        }
    }

    makeConsistent();

    emit connectionRemoved(qnCommon->moduleGUID(), endpoint.id, endpoint.host, endpoint.port);
}

void QnRouter::at_runtimeInfoManager_runtimeInfoAdded(const QnPeerRuntimeInfo &data) {
    foreach (const ec2::ApiConnectionData &connection, data.data.availableConnections)
        addConnection(data.uuid, Endpoint(connection.peerId, connection.host, connection.port));
}

void QnRouter::at_runtimeInfoManager_runtimeInfoChanged(const QnPeerRuntimeInfo &data) {
    m_mutex.lock();
    QList<Endpoint> connections = m_connections.values(data.uuid);
    m_mutex.unlock();

    QList<ec2::ApiConnectionData> newConnections = data.data.availableConnections;

    while (!newConnections.isEmpty()) {
        ec2::ApiConnectionData connection = newConnections.takeFirst();
        Endpoint endpoint(connection.peerId, connection.host, connection.port);
        bool isNew = !connections.removeOne(endpoint);
        if (isNew)
            addConnection(data.uuid, endpoint);
    }

    while (!connections.isEmpty())
        removeConnection(data.uuid, connections.takeFirst());
}

void QnRouter::at_runtimeInfoManager_runtimeInfoRemoved(const QnPeerRuntimeInfo &data) {
    foreach (const ec2::ApiConnectionData &connection, data.data.availableConnections)
        removeConnection(data.uuid, Endpoint(connection.peerId, connection.host, connection.port));
}

bool QnRouter::addConnection(const QUuid &id, const QnRouter::Endpoint &endpoint) {
    QMutexLocker lk(&m_mutex);

    if (m_connections.contains(id, endpoint))
        return false;

    m_connections.insert(id, endpoint);
    m_routeBuilder->addConnection(id, endpoint.id, endpoint.host, endpoint.port);

    lk.unlock();

    NX_LOG(lit("QnRouter. Connection added: %1 -> %2[%3:%4]").arg(id.toString()).arg(endpoint.id.toString()).arg(endpoint.host).arg(endpoint.port), cl_logDEBUG1);
    emit connectionAdded(id, endpoint.id, endpoint.host, endpoint.port);

    return true;
}

bool QnRouter::removeConnection(const QUuid &id, const QnRouter::Endpoint &endpoint) {
    QMutexLocker lk(&m_mutex);

    if (!m_connections.remove(id, endpoint))
        return false;

    m_routeBuilder->removeConnection(id, endpoint.id, endpoint.host, endpoint.port);

    lk.relock();
    makeConsistent();

    lk.unlock();
    NX_LOG(lit("QnRouter. Connection removed: %1 -> %2[%3:%4]").arg(id.toString()).arg(endpoint.id.toString()).arg(endpoint.host).arg(endpoint.port), cl_logDEBUG1);
    emit connectionRemoved(id, endpoint.id, endpoint.host, endpoint.port);

    return true;
}

void QnRouter::makeConsistent() {
    // this function just makes endpoint availability test and removes connections to unavailable endpoints
    QMultiHash<QUuid, Endpoint> connections = m_connections;

    QQueue<QUuid> pointsToCheck;
    QSet<QUuid> checkedPoints;
    pointsToCheck.enqueue(qnCommon->moduleGUID());

    while (!pointsToCheck.isEmpty()) {
        QUuid point = pointsToCheck.dequeue();
        checkedPoints.insert(point);

        foreach (const Endpoint &endpoint, connections.values(point)) {
            connections.remove(point, endpoint);
            if (!checkedPoints.contains(endpoint.id))
                pointsToCheck.enqueue(endpoint.id);
        }
    }

    for (auto it = connections.begin(); it != connections.end(); ++it) {
        m_routeBuilder->removeConnection(it.key(), it->id, it->host, it->port);
        m_connections.remove(it.key(), it.value());
    }
}
