#ifndef GLOBAL_MODULE_FINDER_H
#define GLOBAL_MODULE_FINDER_H

#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <nx_ec/ec_api.h>
#include <utils/network/module_information.h>
#include <utils/common/singleton.h>

class QnModuleFinder;

class QnGlobalModuleFinder : public QObject, public Singleton<QnGlobalModuleFinder> {
    Q_OBJECT
public:
    QnGlobalModuleFinder(QnModuleFinder *moduleFinder = 0, QObject *parent = 0);

    void setConnection(const ec2::AbstractECConnectionPtr &connection);

    static void fillApiModuleData(const QnModuleInformation &moduleInformation, ec2::ApiModuleData *data);
    static void fillFromApiModuleData(const ec2::ApiModuleData &data, QnModuleInformation *moduleInformation);

    QList<QnModuleInformation> foundModules() const;
    QnModuleInformation moduleInformation(const QnUuid &id) const;

signals:
    void peerChanged(const QnModuleInformation &moduleInformation);
    void peerLost(const QnModuleInformation &moduleInformation);

private slots:
    void at_moduleChanged(const QnModuleInformation &moduleInformation, bool isAlive);
    void at_moduleFinder_moduleChanged(const QnModuleInformation &moduleInformation);
    void at_moduleFinder_moduleLost(const QnModuleInformation &moduleInformation);

    void at_router_connectionAdded(const QnUuid &discovererId, const QnUuid &peerId, const QString &host);
    void at_router_connectionRemoved(const QnUuid &discovererId, const QnUuid &peerId, const QString &host);

private:
    void addModule(const QnModuleInformation &moduleInformation);

    QSet<QString> getModuleAddresses(const QnUuid &id) const;
    void updateAddresses(const QnUuid &id);

private:
    mutable QMutex m_mutex;
    std::weak_ptr<ec2::AbstractECConnection> m_connection;  // just to know from where to disconnect
    const QPointer<QnModuleFinder> m_moduleFinder;
    QHash<QnUuid, QnModuleInformation> m_moduleInformationById;
    QHash<QnUuid, QHash<QnUuid, QSet<QString>>> m_discoveredAddresses;
};

#endif // GLOBAL_MODULE_FINDER_H
