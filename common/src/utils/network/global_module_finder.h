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
    QnGlobalModuleFinder(QObject *parent = 0);

    void setConnection(const ec2::AbstractECConnectionPtr &connection);
    void setModuleFinder(QnModuleFinder *moduleFinder);

    static void fillApiModuleData(const QnModuleInformation &moduleInformation, ec2::ApiModuleData *data);
    static void fillFromApiModuleData(const ec2::ApiModuleData &data, QnModuleInformation *moduleInformation);

    QList<QnModuleInformation> foundModules() const;

    QList<QnId> discoverers(const QnId &moduleId);
    QMultiHash<QnId, QnId> discoverers() const;

    QnModuleInformation moduleInformation(const QnId &id) const;

signals:
    void peerFound(const QnModuleInformation &moduleInformation);
    void peerChanged(const QnModuleInformation &moduleInformation);
    void peerLost(const QnModuleInformation &moduleInformation);

private slots:
    void at_moduleChanged(const QnModuleInformation &moduleInformation, bool isAlive, const QnId &discoverer);
    void at_moduleFinder_moduleFound(const QnModuleInformation &moduleInformation);
    void at_moduleFinder_moduleLost(const QnModuleInformation &moduleInformation);

    void at_resourcePool_statusChanged(const QnResourcePtr &resource);

private:
    void addModule(const QnModuleInformation &moduleInformation, const QnId &discoverer);
    void removeModule(const QnModuleInformation &moduleInformation, const QnId &discoverer);

    void removeAllModulesDiscoveredBy(const QnId &discoverer);

private:
    ec2::AbstractECConnectionPtr m_connection;  // just to know from where to disconnect
    QPointer<QnModuleFinder> m_moduleFinder;
    QHash<QnId, QnModuleInformation> m_moduleInformationById;
    QMultiHash<QnId, QnId> m_discovererIdByServerId;
};

#endif // GLOBAL_MODULE_FINDER_H
