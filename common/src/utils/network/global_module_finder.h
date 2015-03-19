#ifndef GLOBAL_MODULE_FINDER_H
#define GLOBAL_MODULE_FINDER_H

#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <nx_ec/ec_api.h>
#include <utils/network/module_information.h>
#include <utils/common/singleton.h>

class QnModuleFinder;
struct QnGlobalModuleInformation : QnModuleInformation {
    QSet<QString> remoteAddresses;

    QnGlobalModuleInformation() {}
    QnGlobalModuleInformation(const QnModuleInformation &other) :
        QnModuleInformation(other)
    {}
};

class QnGlobalModuleFinder : public QObject, public Singleton<QnGlobalModuleFinder> {
    Q_OBJECT
public:
    QnGlobalModuleFinder(QnModuleFinder *moduleFinder = 0, QObject *parent = 0);

    void setConnection(const ec2::AbstractECConnectionPtr &connection);

    QList<QnGlobalModuleInformation> foundModules() const;
    QnGlobalModuleInformation moduleInformation(const QnUuid &id) const;

signals:
    void peerChanged(const QnGlobalModuleInformation &moduleInformation);
    void peerLost(const QnGlobalModuleInformation &moduleInformation);

private slots:
    void at_moduleChanged(const QnModuleInformation &moduleInformation, bool isAlive);
    void at_moduleFinder_moduleChanged(const QnModuleInformation &moduleInformation);
    void at_moduleFinder_moduleLost(const QnModuleInformation &moduleInformation);

private:
    void addModule(const QnModuleInformation &moduleInformation);

    QSet<QString> getModuleAddresses(const QnUuid &id) const;
    void updateAddresses(const QnUuid &id);

private:
    mutable QMutex m_mutex;
    std::weak_ptr<ec2::AbstractECConnection> m_connection;  // just to know from where to disconnect
    const QPointer<QnModuleFinder> m_moduleFinder;
    QHash<QnUuid, QnGlobalModuleInformation> m_moduleInformationById;
    QHash<QnUuid, QHash<QnUuid, QSet<QString>>> m_discoveredAddresses;
};

#endif // GLOBAL_MODULE_FINDER_H
