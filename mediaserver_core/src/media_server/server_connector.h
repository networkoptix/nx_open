#ifndef SERVER_CONNECTOR_H
#define SERVER_CONNECTOR_H

#include <QtCore/QObject>

#include <common/common_module_aware.h>
#include <nx/network/socket_common.h>
#include <nx/utils/singleton.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/discovery/manager.h>

struct QnModuleInformation;

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

    void addConnection(const nx::vms::discovery::ModuleEndpoint& module);
    void removeConnection(const QnUuid& id);

    void start();
    void stop();
    void restart();

private slots:
    void at_moduleFound(nx::vms::discovery::ModuleEndpoint module);
    void at_moduleChanged(nx::vms::discovery::ModuleEndpoint module);
    void at_moduleLost(QnUuid id);

private:
    QnMutex m_mutex;
    QHash<QnUuid, QString> m_urls;
};

#endif // SERVER_CONNECTOR_H
