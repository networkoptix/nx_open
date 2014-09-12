#include "router.h"

#include "utils/common/log.h"
#include "nx_ec/dummy_handler.h"
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

void QnRouter::setConnection(const ec2::AbstractECConnectionPtr &connection) {
    QMutexLocker lk(&m_mutex);

    ec2::AbstractECConnectionPtr oldConnection = m_connection.lock();

    if (oldConnection) {
        oldConnection->getMiscManager()->disconnect(this);

        QUuid localId = qnCommon->moduleGUID();
        for (auto it = m_connections.begin(); it != m_connections.end(); /* no increment */) {
            if (it.key() == localId) {
                ++it;
                continue;
            }

            m_routeBuilder->removeConnection(it.key(), it->id, it->host, it->port);
            it = m_connections.erase(it);
        }
    }

    m_connection = connection;

    if (connection) {
        connect(connection->getMiscManager().get(),         &ec2::AbstractMiscManager::connectionAdded,     this,   &QnRouter::at_connectionAdded);
        connect(connection->getMiscManager().get(),         &ec2::AbstractMiscManager::connectionRemoved,   this,   &QnRouter::at_connectionRemoved);
    }
}

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

void QnRouter::at_connectionAdded(const QUuid &discovererId, const QUuid &peerId, const QString &host, quint16 port) {
    if (discovererId == peerId)
        return;

    QMutexLocker lk(&m_mutex);

    if (m_connections.contains(discovererId, Endpoint(peerId, host, port)))
        return;

    m_connections.insert(discovererId, Endpoint(peerId, host, port));
    m_routeBuilder->addConnection(discovererId, peerId, host, port);

    lk.unlock();
    NX_LOG(lit("QnRouter. Connection added: %1 -> %2[%3:%4]").arg(discovererId.toString()).arg(peerId.toString()).arg(host).arg(port), cl_logDEBUG1);
    emit connectionAdded(discovererId, peerId, host, port);
}

void QnRouter::at_connectionRemoved(const QUuid &discovererId, const QUuid &peerId, const QString &host, quint16 port) {
    QMutexLocker lk(&m_mutex);

    if (!m_connections.contains(discovererId, Endpoint(peerId, host, port)))
        return;

    m_routeBuilder->removeConnection(discovererId, peerId, host, port);
    m_connections.remove(discovererId, Endpoint(peerId, host, port));
    makeConsistent();

    lk.unlock();
    NX_LOG(lit("QnRouter. Connection removed: %1 -> %2[%3:%4]").arg(discovererId.toString()).arg(peerId.toString()).arg(host).arg(port), cl_logDEBUG1);
    emit connectionRemoved(discovererId, peerId, host, port);
}

void QnRouter::at_moduleFinder_moduleUrlFound(const QnModuleInformation &moduleInformation, const QUrl &url) {
    if (moduleInformation.id == qnCommon->moduleGUID())
        return;

    QMutexLocker lk(&m_mutex);

    Endpoint endpoint(moduleInformation.id, url.host(), url.port());
    if (m_connections.contains(qnCommon->moduleGUID(), endpoint))
        return;

    m_connections.insert(qnCommon->moduleGUID(), endpoint);
    m_routeBuilder->addConnection(qnCommon->moduleGUID(), endpoint.id, endpoint.host, endpoint.port);

    if (!m_passive) {
        if (ec2::AbstractECConnectionPtr connection = m_connection.lock()) {
            lk.unlock();
            connection->getMiscManager()->addConnection(qnCommon->moduleGUID(), endpoint.id, endpoint.host, endpoint.port, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
        }
    }
    lk.unlock();
    NX_LOG(lit("QnRouter. Connection added: %1 -> %2[%3:%4]").arg(qnCommon->moduleGUID().toString()).arg(endpoint.id.toString()).arg(endpoint.host).arg(endpoint.port), cl_logDEBUG1);
    emit connectionAdded(qnCommon->moduleGUID(), endpoint.id, endpoint.host, endpoint.port);
}


void QnRouter::at_moduleFinder_moduleUrlLost(const QnModuleInformation &moduleInformation, const QUrl &url) {
    QMutexLocker lk(&m_mutex);

    Endpoint endpoint(moduleInformation.id, url.host(), url.port());
    if (!m_connections.remove(qnCommon->moduleGUID(), endpoint))
        return;

    m_routeBuilder->removeConnection(qnCommon->moduleGUID(), endpoint.id, endpoint.host, endpoint.port);

    if (!m_passive) {
        if (ec2::AbstractECConnectionPtr connection = m_connection.lock()) {
            lk.unlock();
            connection->getMiscManager()->removeConnection(qnCommon->moduleGUID(), endpoint.id, endpoint.host, endpoint.port, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
        }
    }

    lk.relock();
    makeConsistent();

    lk.unlock();
    NX_LOG(lit("QnRouter. Connection removed: %1 -> %2[%3:%4]").arg(qnCommon->moduleGUID().toString()).arg(endpoint.id.toString()).arg(endpoint.host).arg(endpoint.port), cl_logDEBUG1);
    emit connectionRemoved(qnCommon->moduleGUID(), endpoint.id, endpoint.host, endpoint.port);
}

void QnRouter::makeConsistent() {
    // this function just makes endpoint availability test and removes connections to unavailable endpoints
    QMultiHash<QUuid, Endpoint> connections = m_connections;

    QQueue<QUuid> pointsToCheck;
    pointsToCheck.enqueue(qnCommon->moduleGUID());

    while (!pointsToCheck.isEmpty()) {
        QUuid point = pointsToCheck.dequeue();

        foreach (const Endpoint &endpoint, connections.values(point)) {
            connections.remove(point, endpoint);
            if (!pointsToCheck.contains(endpoint.id))
                pointsToCheck.enqueue(endpoint.id);
        }
    }

    for (auto it = connections.begin(); it != connections.end(); ++it) {
        m_routeBuilder->removeConnection(it.key(), it->id, it->host, it->port);
        m_connections.remove(it.key(), it.value());
    }
}
