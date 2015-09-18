#ifndef SERVER_CONNECTOR_H
#define SERVER_CONNECTOR_H

#include <QtCore/QObject>
#include <utils/thread/mutex.h>
#include <utils/common/singleton.h>
#include <utils/network/socket_common.h>

class QnModuleFinder;
struct QnModuleInformation;
class SocketAddress;

class QnServerConnector : public QObject, public Singleton<QnServerConnector> {
    Q_OBJECT

    struct AddressInfo {
        QString urlString;
        QnUuid peerId;
    };

public:
    explicit QnServerConnector(QnModuleFinder *moduleFinder, QObject *parent = 0);

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
    const QnModuleFinder *m_moduleFinder;
    QHash<QString, AddressInfo> m_usedAddresses;
};

#endif // SERVER_CONNECTOR_H
