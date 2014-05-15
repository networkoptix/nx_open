#ifndef GLOBAL_MODULE_FINDER_H
#define GLOBAL_MODULE_FINDER_H

#include <QtCore/QObject>

#include <nx_ec/ec_api.h>
#include <utils/network/module_information.h>

class QnGlobalModuleFinder : public QObject {
    Q_OBJECT
public:
    static QnGlobalModuleFinder *instance();

    QnGlobalModuleFinder(QObject *parent = 0);

    void setConnection(const ec2::AbstractECConnectionPtr &connection);

signals:
    void peerFound(const QnModuleInformation &moduleInformation);
    void peerChanged(const QnModuleInformation &moduleInformation);
    void peerLost(const QnModuleInformation &moduleInformation);

private slots:
    void at_peerFound(ec2::ApiServerAliveData data, bool isProxy);
    void at_peerLost(ec2::ApiServerAliveData data, bool isProxy);

private:
    ec2::AbstractECConnectionPtr m_connection;  // just to know from where to disconnect
    QHash<QString, QnModuleInformation> m_moduleInformationById;

    QnGlobalModuleFinder *s_instance;
};

#endif // GLOBAL_MODULE_FINDER_H
