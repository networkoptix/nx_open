#pragma once

#include "nx_ec/ec_api.h"

#include <transaction/transaction_log.h>

namespace ec2 {

// TODO: #2.4 remove EC prefix to avoid ec2::ECConnectionNotificationManager
//!Stands for emitting API notifications
class ECConnectionNotificationManager
{
public:
    ECConnectionNotificationManager(
        AbstractECConnection* ecConnection,
        QnLicenseNotificationManager* licenseManager,
        QnResourceNotificationManager* resourceManager,
        QnMediaServerNotificationManager* mediaServerManager,
        QnCameraNotificationManager* cameraManager,
        QnUserNotificationManager* userManager,
        QnBusinessEventNotificationManager* businessEventManager,
        QnLayoutNotificationManager* layoutManager,
        QnLayoutTourNotificationManager* layoutTourManager,
        QnVideowallNotificationManager* videowallManager,
        QnWebPageNotificationManager* webPageManager,
        QnStoredFileNotificationManager* storedFileManager,
        QnUpdatesNotificationManager* updatesManager,
        QnMiscNotificationManager* miscManager,
        QnDiscoveryNotificationManager* discoveryManager);

    template<typename T>
    void triggerNotification(const QnTransaction<T> &tran, NotificationSource source)
    {
        ec2::detail::NotificationParams notificationParams = {
            m_ecConnection,
            m_licenseManager,
            m_resourceManager,
            m_mediaServerManager,
            m_cameraManager,
            m_userManager,
            m_businessEventManager,
            m_layoutManager,
            m_layoutTourManager,
            m_videowallManager,
            m_webPageManager,
            m_storedFileManager,
            m_updatesManager,
            m_miscManager,
            m_discoveryManager,
            source
        };

        auto tdBase = getTransactionDescriptorByValue(tran.command);
        auto td = dynamic_cast<detail::TransactionDescriptor<T>*>(tdBase);
        NX_ASSERT(td, "Downcast to TransactionDescriptor<TransactionParams>* failed");
        if (td == nullptr)
            return;
        td->triggerNotificationFunc(tran, notificationParams);
    }

private:
    AbstractECConnection* m_ecConnection;
    QnLicenseNotificationManager* m_licenseManager;
    QnResourceNotificationManager* m_resourceManager;
    QnMediaServerNotificationManager* m_mediaServerManager;
    QnCameraNotificationManager* m_cameraManager;
    QnUserNotificationManager* m_userManager;
    QnBusinessEventNotificationManager* m_businessEventManager;
    QnLayoutNotificationManager* m_layoutManager;
    QnLayoutTourNotificationManager* m_layoutTourManager;
    QnVideowallNotificationManager* m_videowallManager;
    QnWebPageNotificationManager* m_webPageManager;
    QnStoredFileNotificationManager* m_storedFileManager;
    QnUpdatesNotificationManager* m_updatesManager;
    QnMiscNotificationManager* m_miscManager;
    QnDiscoveryNotificationManager* m_discoveryManager;
};

} // namespace ec2
