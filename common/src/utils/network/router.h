#ifndef ROUTER_H
#define ROUTER_H

#include <QtCore/QObject>

#include <nx_ec/ec_api.h>
#include <utils/common/singleton.h>
#include <utils/network/route.h>

class QnRouteBuilder;
class QnModuleFinder;
struct QnModuleInformation;

class QnRouter : public QObject, public Singleton<QnRouter> {
    Q_OBJECT
public:
    struct Endpoint {
        QUuid id;
        QString host;
        quint16 port;

        Endpoint(const QUuid &id, const QString &host = QString(), quint16 port = 0) : id(id), host(host), port(port) {}
        bool operator ==(const Endpoint &other) const { return id == other.id && host == other.host && port == other.port; }
    };

    explicit QnRouter(QObject *parent = 0);
    ~QnRouter();

    void setConnection(const ec2::AbstractECConnectionPtr &connection);
    void setModuleFinder(QnModuleFinder *moduleFinder);

    QMultiHash<QUuid, Endpoint> connections() const;
    QHash<QUuid, QnRouteList> routes() const;

    QnRoute routeTo(const QUuid &id) const;
    QnRoute routeTo(const QString &host, quint16 port) const;

    QUuid whoIs(const QString &host, quint16 port) const;

private slots:
    void at_connectionAdded(const QUuid &discovererId, const QUuid &peerId, const QString &host, quint16 port);
    void at_connectionRemoved(const QUuid &discovererId, const QUuid &peerId, const QString &host, quint16 port);
    void at_moduleFinder_moduleFound(const QnModuleInformation &moduleInformation);
    void at_moduleFinder_moduleLost(const QnModuleInformation &moduleInformation);

private:
    void makeConsistent();

private:
    std::weak_ptr<ec2::AbstractECConnection> m_connection;
    QnModuleFinder *m_moduleFinder;
    QScopedPointer<QnRouteBuilder> m_routeBuilder;
    QMultiHash<QUuid, Endpoint> m_connections;
};

#endif // ROUTER_H
