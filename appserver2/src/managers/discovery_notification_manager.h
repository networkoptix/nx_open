#pragma once

#include <nx_ec/ec_api.h>
#include <transaction/transaction.h>

namespace ec2
{

class QnDiscoveryNotificationManager :
    public AbstractDiscoveryNotificationManager,
    public QnCommonModuleAware
{
public:
    QnDiscoveryNotificationManager(QnCommonModule* commonModule);

    void triggerNotification(const QnTransaction<ApiDiscoverPeerData> &transaction, NotificationSource source);
    void triggerNotification(const QnTransaction<ApiDiscoveryData> &transaction, NotificationSource source);
    void triggerNotification(const ApiDiscoveryData &discoveryData, bool addInformation = true);
    void triggerNotification(const QnTransaction<ApiDiscoveryDataList> &tran, NotificationSource source);
    void triggerNotification(const QnTransaction<ApiDiscoveredServerData> &tran, NotificationSource source);
    void triggerNotification(const QnTransaction<ApiDiscoveredServerDataList> &tran, NotificationSource source);
};

typedef std::shared_ptr<QnDiscoveryNotificationManager> QnDiscoveryNotificationManagerPtr;

} // namespace ec2
