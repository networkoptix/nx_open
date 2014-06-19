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
        QnId id;
        QString host;
        quint16 port;

        Endpoint(const QnId &id, const QString &host = QString(), quint16 port = 0) : id(id), host(host), port(port) {}
        bool operator ==(const Endpoint &other) const { return id == other.id && host == other.host && port == other.port; }
    };

    explicit QnRouter(QObject *parent = 0);
    ~QnRouter();

    void setConnection(const ec2::AbstractECConnectionPtr &connection);
    void setModuleFinder(QnModuleFinder *moduleFinder);

    QMultiHash<QnId, Endpoint> connections() const;
    QHash<QnId, QnRouteList> routes() const;

    QnRoute routeTo(const QnId &id) const;
    QnRoute routeTo(const QString &host, quint16 port) const;

    QnId whoIs(const QString &host, quint16 port) const;

private slots:
    void at_connectionAdded(const QnId &discovererId, const QnId &peerId, const QString &host, quint16 port);
    void at_connectionRemoved(const QnId &discovererId, const QnId &peerId, const QString &host, quint16 port);
    void at_moduleFinder_moduleFound(const QnModuleInformation &moduleInformation);
    void at_moduleFinder_moduleLost(const QnModuleInformation &moduleInformation);

private:
    void makeConsistent();

private:
    ec2::AbstractECConnectionPtr m_connection;
    QnModuleFinder *m_moduleFinder;
    QScopedPointer<QnRouteBuilder> m_routeBuilder;
    QMultiHash<QnId, Endpoint> m_connections;
};

#endif // ROUTER_H
