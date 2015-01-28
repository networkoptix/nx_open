#include "router.h"

#include <QtCore/QElapsedTimer>

#include "utils/common/log.h"
#include "nx_ec/dummy_handler.h"
#include "nx_ec/ec_api.h"
#include "common/common_module.h"
#include "route_builder.h"
#include "module_finder.h"

namespace {
    static const int runtimeDataUpdateTimeout = 3000;
} // anonymous namespace

QnRouter::QnRouter(QnModuleFinder *moduleFinder, bool passive, QObject *parent) :
    QObject(parent),
    m_mutex(QMutex::Recursive),
    m_connection(std::weak_ptr<ec2::AbstractECConnection>()),
    m_routeBuilder(new QnRouteBuilder(qnCommon->moduleGUID())),
    m_passive(passive),
    m_runtimeDataUpdateTimer(new QTimer(this)),
    m_runtimeDataElapsedTimer(new QElapsedTimer())
{
    connect(moduleFinder,       &QnModuleFinder::moduleUrlFound,    this,   &QnRouter::at_moduleFinder_moduleUrlFound);
    connect(moduleFinder,       &QnModuleFinder::moduleUrlLost,     this,   &QnRouter::at_moduleFinder_moduleUrlLost);

    QnRuntimeInfoManager *runtimeInfoManager = QnRuntimeInfoManager::instance();
    connect(runtimeInfoManager, &QnRuntimeInfoManager::runtimeInfoAdded,    this,   &QnRouter::at_runtimeInfoManager_runtimeInfoAdded);
    connect(runtimeInfoManager, &QnRuntimeInfoManager::runtimeInfoChanged,  this,   &QnRouter::at_runtimeInfoManager_runtimeInfoChanged);
    connect(runtimeInfoManager, &QnRuntimeInfoManager::runtimeInfoRemoved,  this,   &QnRouter::at_runtimeInfoManager_runtimeInfoRemoved);

    m_runtimeDataUpdateTimer = new QTimer(this);
    m_runtimeDataUpdateTimer->setInterval(runtimeDataUpdateTimeout);
    m_runtimeDataUpdateTimer->setSingleShot(true);
    connect(m_runtimeDataUpdateTimer, &QTimer::timeout, this, [this](){ updateRuntimeData(true); });
    m_runtimeDataElapsedTimer->start();
}

QnRouter::~QnRouter() {}

QMultiHash<QnUuid, QnRouter::Endpoint> QnRouter::connections() const {
    QMutexLocker lk(&m_mutex);
    return m_connections;
}

QHash<QnUuid, QnRoute> QnRouter::routes() const {
    QMutexLocker lk(&m_mutex);
    return m_routeBuilder->routes();
}

QnRoute QnRouter::routeTo(const QnUuid &id, const QnUuid &via) const {
    QMutexLocker lk(&m_mutex);
    return m_routeBuilder->routeTo(id, via);
}

QnRoute QnRouter::routeTo(const QString &host, quint16 port) const {
    QnUuid id = whoIs(host, port);
    if (id.isNull())
        return QnRoute();

    QnRoute route = routeTo(id);

    /* Ensure the latest point is the requested point */
    if (route.isValid() && route.points.last().host != host) {
        QnUuid from = route.length() > 1 ? route.points[route.length() - 2].peerId : qnCommon->moduleGUID();
        if (m_connections.contains(from, Endpoint(id, host, port)))
            route.points.last() = QnRoutePoint(id, host, port);
    }

    return route;
}

QnUuid QnRouter::whoIs(const QString &host, quint16 port) const {
    QMutexLocker lk(&m_mutex);

    for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
        if (it->host == host && it->port == port)
            return it->id;
    }
    return QnUuid();
}

void QnRouter::at_moduleFinder_moduleUrlFound(const QnModuleInformation &moduleInformation, const QUrl &url) {
    Endpoint endpoint(moduleInformation.id, url.host(), url.port());

    if (endpoint.id.isNull())
        return;

    if (!addConnection(qnCommon->moduleGUID(), endpoint))
        return;

    if (!m_passive)
        updateRuntimeData();

    emit connectionAdded(qnCommon->moduleGUID(), endpoint.id, endpoint.host, endpoint.port);
}

void QnRouter::at_moduleFinder_moduleUrlLost(const QnModuleInformation &moduleInformation, const QUrl &url) {
    Endpoint endpoint(moduleInformation.id, url.host(), url.port());

    if (!removeConnection(qnCommon->moduleGUID(), endpoint))
        return;

    if (!m_passive)
        updateRuntimeData();

    emit connectionRemoved(qnCommon->moduleGUID(), endpoint.id, endpoint.host, endpoint.port);
}

void QnRouter::at_runtimeInfoManager_runtimeInfoAdded(const QnPeerRuntimeInfo &data) {
    if (data.uuid == qnCommon->moduleGUID())
        return;

    for (const ec2::ApiConnectionData &connection: data.data.availableConnections)
        addConnection(data.uuid, Endpoint(connection.peerId, connection.host, connection.port));
}

void QnRouter::at_runtimeInfoManager_runtimeInfoChanged(const QnPeerRuntimeInfo &data) {
    if (data.uuid == qnCommon->moduleGUID())
        return;

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
    if (data.uuid == qnCommon->moduleGUID())
        return;

    for (const ec2::ApiConnectionData &connection: data.data.availableConnections)
        removeConnection(data.uuid, Endpoint(connection.peerId, connection.host, connection.port));
}

bool QnRouter::addConnection(const QnUuid &id, const QnRouter::Endpoint &endpoint) {
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

bool QnRouter::removeConnection(const QnUuid &id, const QnRouter::Endpoint &endpoint) {
    QMutexLocker lk(&m_mutex);

    if (!m_connections.remove(id, endpoint))
        return false;

    m_routeBuilder->removeConnection(id, endpoint.id, endpoint.host, endpoint.port);

    lk.unlock();

    NX_LOG(lit("QnRouter. Connection removed: %1 -> %2[%3:%4]").arg(id.toString()).arg(endpoint.id.toString()).arg(endpoint.host).arg(endpoint.port), cl_logDEBUG1);
    emit connectionRemoved(id, endpoint.id, endpoint.host, endpoint.port);

    return true;
}

void QnRouter::updateRuntimeData(bool force) {
    QnRuntimeInfoManager *runtimeInfoManager = QnRuntimeInfoManager::instance();
    if (!runtimeInfoManager)
        return;

    if (!force && !m_runtimeDataElapsedTimer->hasExpired(runtimeDataUpdateTimeout)) {
        if (!m_runtimeDataUpdateTimer->isActive())
            m_runtimeDataUpdateTimer->start();
        return;
    }

    m_runtimeDataUpdateTimer->stop();

    QList<ec2::ApiConnectionData> connections;
    for (const Endpoint &endpoint: m_connections.values(qnCommon->moduleGUID())) {
        ec2::ApiConnectionData connection;
        connection.peerId = endpoint.id;
        connection.host = endpoint.host;
        connection.port = endpoint.port;
        connections.append(connection);
    }

    QnPeerRuntimeInfo localInfo = runtimeInfoManager->localInfo();
    localInfo.data.availableConnections = connections;
    runtimeInfoManager->updateLocalItem(localInfo);

    m_runtimeDataElapsedTimer->start();
}
