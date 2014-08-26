#include "router.h"

#include "nx_ec/dummy_handler.h"
#include "common/common_module.h"
#include "route_builder.h"
#include "module_finder.h"

QnRouter::QnRouter(QObject *parent) :
    QObject(parent),
    m_connection(std::weak_ptr<ec2::AbstractECConnection>()),
    m_moduleFinder(0),
    m_routeBuilder(new QnRouteBuilder(qnCommon->moduleGUID()))
{
}

QnRouter::~QnRouter() {}

void QnRouter::setConnection(const ec2::AbstractECConnectionPtr &connection) {
    ec2::AbstractECConnectionPtr oldConnection = m_connection.lock();

    if (oldConnection)
        oldConnection->getMiscManager()->disconnect(this);

    m_connection = connection;

    if (connection) {
        connect(connection->getMiscManager().get(),         &ec2::AbstractMiscManager::connectionAdded,     this,   &QnRouter::at_connectionAdded);
        connect(connection->getMiscManager().get(),         &ec2::AbstractMiscManager::connectionRemoved,   this,   &QnRouter::at_connectionRemoved);
    }
}

void QnRouter::setModuleFinder(QnModuleFinder *moduleFinder) {
    if (m_moduleFinder)
        m_moduleFinder->disconnect(this);

    m_moduleFinder = moduleFinder;

    if (moduleFinder) {
        foreach (const QnModuleInformation &moduleInformation, moduleFinder->foundModules())
            at_moduleFinder_moduleFound(moduleInformation);

        connect(moduleFinder,               &QnModuleFinder::moduleFound,   this,   &QnRouter::at_moduleFinder_moduleFound);
        connect(moduleFinder,               &QnModuleFinder::moduleLost,    this,   &QnRouter::at_moduleFinder_moduleLost);
    }
}

QMultiHash<QUuid, QnRouter::Endpoint> QnRouter::connections() const {
    return m_connections;
}

QHash<QUuid, QnRouteList> QnRouter::routes() const {
    return m_routeBuilder->routes();
}

QnRoute QnRouter::routeTo(const QUuid &id) const {
    return m_routeBuilder->routeTo(id);
}

QnRoute QnRouter::routeTo(const QString &host, quint16 port) const {
    QUuid id = whoIs(host, port);
    return id.isNull() ? QnRoute() : routeTo(id);
}

QUuid QnRouter::whoIs(const QString &host, quint16 port) const {
    for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
        if (it->host == host && it->port == port)
            return it->id;
    }
    return QUuid();
}

void QnRouter::at_connectionAdded(const QUuid &discovererId, const QUuid &peerId, const QString &host, quint16 port) {
    if (m_connections.contains(discovererId, Endpoint(peerId, host, port)))
        return;

    m_connections.insert(discovererId, Endpoint(peerId, host, port));
    m_routeBuilder->addConnection(discovererId, peerId, host, port);
}

void QnRouter::at_connectionRemoved(const QUuid &discovererId, const QUuid &peerId, const QString &host, quint16 port) {
    if (!m_connections.contains(discovererId, Endpoint(peerId, host, port)))
        return;

    m_routeBuilder->removeConnection(discovererId, peerId, host, port);
    m_connections.remove(discovererId, Endpoint(peerId, host, port));
    makeConsistent();
}

void QnRouter::at_moduleFinder_moduleFound(const QnModuleInformation &moduleInformation) {
    foreach (const QString &address, moduleInformation.remoteAddresses) {
        Endpoint endpoint(moduleInformation.id, address, moduleInformation.port);
        if (m_connections.contains(qnCommon->moduleGUID(), endpoint))
            return;

        m_connections.insert(qnCommon->moduleGUID(), endpoint);
        m_routeBuilder->addConnection(qnCommon->moduleGUID(), endpoint.id, endpoint.host, endpoint.port);
        if (ec2::AbstractECConnectionPtr connection = m_connection.lock())
            connection->getMiscManager()->addConnection(qnCommon->moduleGUID(), endpoint.id, endpoint.host, endpoint.port, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
    }
}

void QnRouter::at_moduleFinder_moduleLost(const QnModuleInformation &moduleInformation) {
    foreach (const Endpoint &endpoint, m_connections.values(qnCommon->moduleGUID())) {
        if (endpoint.id != moduleInformation.id)
            continue;

        m_routeBuilder->removeConnection(qnCommon->moduleGUID(), endpoint.id, endpoint.host, endpoint.port);
        m_connections.remove(qnCommon->moduleGUID(), endpoint);
        if (ec2::AbstractECConnectionPtr connection = m_connection.lock())
            connection->getMiscManager()->removeConnection(qnCommon->moduleGUID(), endpoint.id, endpoint.host, endpoint.port, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
    }
    makeConsistent();
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
