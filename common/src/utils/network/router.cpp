#include "router.h"

#include "nx_ec/dummy_handler.h"
#include "common/common_module.h"
#include "route_builder.h"
#include "module_finder.h"

QnRouter::QnRouter(QObject *parent) :
    QObject(parent),
    m_connection(0),
    m_moduleFinder(0),
    m_routeBuilder(new QnRouteBuilder(qnCommon->moduleGUID()))
{
}

QnRouter::~QnRouter() {}

void QnRouter::setConnection(const ec2::AbstractECConnectionPtr &connection) {
    if (m_connection)
        m_connection->getMiscManager()->disconnect(this);

    m_connection = connection;

    if (m_connection) {
        connect(m_connection->getMiscManager().get(),       &ec2::AbstractMiscManager::connectionAdded,     this,   &QnRouter::at_connectionAdded);
        connect(m_connection->getMiscManager().get(),       &ec2::AbstractMiscManager::connectionRemoved,   this,   &QnRouter::at_connectionRemoved);
    }
}

void QnRouter::setModuleFinder(QnModuleFinder *moduleFinder) {
    if (m_moduleFinder)
        m_moduleFinder->disconnect(this);

    m_moduleFinder = moduleFinder;

    if (moduleFinder) {
        foreach (const QnModuleInformation &moduleInformation, moduleFinder->revealedModules())
            at_moduleFinder_moduleFound(moduleInformation);

        connect(moduleFinder,               &QnModuleFinder::moduleFound,   this,   &QnRouter::at_moduleFinder_moduleFound);
        connect(moduleFinder,               &QnModuleFinder::moduleLost,    this,   &QnRouter::at_moduleFinder_moduleLost);
    }
}

QMultiHash<QnId, QnRouter::Endpoint> QnRouter::connections() const {
    return m_connections;
}

QHash<QnId, QnRouteList> QnRouter::routes() const {
    return m_routeBuilder->routes();
}

QnRoute QnRouter::routeTo(const QnId &id) const {
    return m_routeBuilder->routeTo(id);
}

QnRoute QnRouter::routeTo(const QString &host, quint16 port) const {
    for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
        if (it->host == host && it->port == port)
            return routeTo(it->id);
    }
    return QnRoute();
}

void QnRouter::at_connectionAdded(const QnId &discovererId, const QnId &peerId, const QString &host, quint16 port) {
    if (m_connections.contains(discovererId, Endpoint(peerId, host, port)))
        return;

    m_connections.insert(discovererId, Endpoint(peerId, host, port));
    m_routeBuilder->addConnection(discovererId, peerId, host, port);
}

void QnRouter::at_connectionRemoved(const QnId &discovererId, const QnId &peerId, const QString &host, quint16 port) {
    if (!m_connections.contains(discovererId, Endpoint(peerId, host, port)))
        return;

    m_routeBuilder->removeConnection(discovererId, peerId, host, port);
    m_connections.remove(discovererId, Endpoint(peerId, host, port));
    makeConsistent();
}

void QnRouter::at_moduleFinder_moduleFound(const QnModuleInformation &moduleInformation) {
    foreach (const QString &address, moduleInformation.remoteAddresses) {
        Endpoint endpoint(moduleInformation.id, address, moduleInformation.parameters.value(lit("port")).toUShort());
        if (m_connections.contains(qnCommon->moduleGUID(), endpoint))
            return;

        m_connections.insert(qnCommon->moduleGUID(), endpoint);
        m_routeBuilder->addConnection(qnCommon->moduleGUID(), endpoint.id, endpoint.host, endpoint.port);
        if (m_connection)
            m_connection->getMiscManager()->addConnection(qnCommon->moduleGUID(), endpoint.id, endpoint.host, endpoint.port, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
    }
}

void QnRouter::at_moduleFinder_moduleLost(const QnModuleInformation &moduleInformation) {
    foreach (const Endpoint &endpoint, m_connections.values(qnCommon->moduleGUID())) {
        if (endpoint.id != moduleInformation.id)
            continue;

        m_routeBuilder->removeConnection(qnCommon->moduleGUID(), endpoint.id, endpoint.host, endpoint.port);
        m_connections.remove(qnCommon->moduleGUID(), endpoint);
        if (m_connection)
            m_connection->getMiscManager()->removeConnection(qnCommon->moduleGUID(), endpoint.id, endpoint.host, endpoint.port, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
    }
    makeConsistent();
}

void QnRouter::makeConsistent() {
    // this function just makes endpoint availability test and removes connections to unavailable endpoints
    QMultiHash<QnId, Endpoint> connections = m_connections;

    QQueue<QnId> pointsToCheck;
    pointsToCheck.enqueue(qnCommon->moduleGUID());

    while (!pointsToCheck.isEmpty()) {
        QnId point = pointsToCheck.dequeue();

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
