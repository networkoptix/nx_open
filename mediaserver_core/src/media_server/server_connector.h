#ifndef SERVER_CONNECTOR_H
#define SERVER_CONNECTOR_H

#include <QtCore/QObject>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/singleton.h>
#include <nx/network/socket_common.h>
#include <common/common_module_aware.h>

class QnModuleFinder;
struct QnModuleInformation;
class SocketAddress;

class QnServerConnector:
    public QObject,
    public Singleton<QnServerConnector>,
    public QnCommonModuleAware
{
    Q_OBJECT

    struct AddressInfo
    {
        QString urlString;
        QnUuid peerId;
    };

public:
    explicit QnServerConnector(QnCommonModule* commonModule);

    void addConnection(const QnModuleInformation &moduleInformation, const SocketAddress &address);
    void removeConnection(const QnModuleInformation &moduleInformation, const SocketAddress &address);

    void start();
    void stop();
    void restart();

private slots:
    void at_moduleFinder_moduleAddressFound(const QnModuleInformation &moduleInformation, const SocketAddress &address);
    void at_moduleFinder_moduleAddressLost(const QnModuleInformation &moduleInformation, const SocketAddress &address);
    void at_moduleFinder_moduleChanged(const QnModuleInformation &moduleInformation);

private:
    QnMutex m_mutex;
    QHash<QString, AddressInfo> m_usedAddresses;
};

#endif // SERVER_CONNECTOR_H
