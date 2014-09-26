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

    QSet<QnUuid> discoverers(const QnUuid &moduleId);

    QnModuleInformation moduleInformation(const QnUuid &id) const;

signals:
    void peerChanged(const QnModuleInformation &moduleInformation);
    void peerLost(const QnModuleInformation &moduleInformation);

private slots:
    void at_moduleChanged(const QnModuleInformation &moduleInformation, bool isAlive, const QnUuid &discoverer);
    void at_moduleFinder_moduleChanged(const QnModuleInformation &moduleInformation);
    void at_moduleFinder_moduleLost(const QnModuleInformation &moduleInformation);

    void at_resourcePool_statusChanged(const QnResourcePtr &resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);

private:
    void addModule(const QnModuleInformation &moduleInformation, const QnUuid &discoverer);
    void removeModule(const QnModuleInformation &moduleInformation, const QnUuid &discoverer);

    void removeAllModulesDiscoveredBy(const QnUuid &discoverer);
    QSet<QString> getModuleAddresses(const QnUuid &id) const;

private:
    std::weak_ptr<ec2::AbstractECConnection> m_connection;  // just to know from where to disconnect
    QPointer<QnModuleFinder> m_moduleFinder;
    QHash<QnUuid, QnModuleInformation> m_moduleInformationById;
    QHash<QnUuid, QSet<QnUuid>> m_discovererIdByServerId;
    QHash<QnUuid, QHash<QnUuid, QSet<QString>>> m_discoveredAddresses;
};

#endif // GLOBAL_MODULE_FINDER_H
