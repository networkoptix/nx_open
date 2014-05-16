#ifndef GLOBAL_MODULE_FINDER_H
#define GLOBAL_MODULE_FINDER_H

#include <QtCore/QObject>

#include <nx_ec/ec_api.h>
#include <utils/network/module_information.h>
#include <utils/common/singleton.h>

class QnGlobalModuleFinder : public QObject, public Singleton<QnGlobalModuleFinder> {
    Q_OBJECT
public:
    QnGlobalModuleFinder(QObject *parent = 0);

    void setConnection(const ec2::AbstractECConnectionPtr &connection);

signals:
    void peerFound(const QnModuleInformation &moduleInformation);
    void peerChanged(const QnModuleInformation &moduleInformation);
    void peerLost(const QnModuleInformation &moduleInformation);

private slots:
    void at_peerFound(ec2::ApiServerAliveData data);
    void at_peerLost(ec2::ApiServerAliveData data);

private:
    ec2::AbstractECConnectionPtr m_connection;  // just to know from where to disconnect
    QHash<QnId, QnModuleInformation> m_moduleInformationById;
};

#endif // GLOBAL_MODULE_FINDER_H
