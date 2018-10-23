#pragma once

#include "nx_ec/ec_api.h"

#include <managers/license_notification_manager.h>
#include <managers/resource_notification_manager.h>
#include <managers/media_server_notification_manager.h>
#include <managers/camera_notification_manager.h>
#include <managers/user_notification_manager.h>
#include <managers/event_rules_notification_manager.h>
#include <managers/layout_notification_manager.h>
#include <managers/layout_tour_notification_manager.h>
#include <managers/videowall_notification_manager.h>
#include <managers/webpage_notification_manager.h>
#include <managers/stored_file_notification_manager.h>
#include <managers/updates_notification_manager.h>
#include <managers/misc_notification_manager.h>
#include <managers/discovery_notification_manager.h>
#include <managers/time_notification_manager.h>
#include <managers/analytics_notification_manager.h>

#include <transaction/transaction_descriptor.h>

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
        QnTimeNotificationManager* timeManager,
        QnBusinessEventNotificationManager* businessEventManager,
        QnLayoutNotificationManager* layoutManager,
        QnLayoutTourNotificationManager* layoutTourManager,
        QnVideowallNotificationManager* videowallManager,
        QnWebPageNotificationManager* webPageManager,
        QnStoredFileNotificationManager* storedFileManager,
        QnUpdatesNotificationManager* updatesManager,
        QnMiscNotificationManager* miscManager,
        QnDiscoveryNotificationManager* discoveryManager,
        AnalyticsNotificationManager* analyticsManager);

    template<typename T>
    void triggerNotification(const QnTransaction<T> &tran, ec2::NotificationSource source)
    {
        ec2::detail::NotificationParams notificationParams = {
            m_ecConnection,
            m_licenseManager,
            m_resourceManager,
            m_mediaServerManager,
            m_cameraManager,
            m_userManager,
            m_timeManager,
            m_businessEventManager,
            m_layoutManager,
            m_layoutTourManager,
            m_videowallManager,
            m_webPageManager,
            m_storedFileManager,
            m_updatesManager,
            m_miscManager,
            m_discoveryManager,
            m_analyticsManager,
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
    QnTimeNotificationManager* m_timeManager;
    QnBusinessEventNotificationManager* m_businessEventManager;
    QnLayoutNotificationManager* m_layoutManager;
    QnLayoutTourNotificationManager* m_layoutTourManager;
    QnVideowallNotificationManager* m_videowallManager;
    QnWebPageNotificationManager* m_webPageManager;
    QnStoredFileNotificationManager* m_storedFileManager;
    QnUpdatesNotificationManager* m_updatesManager;
    QnMiscNotificationManager* m_miscManager;
    QnDiscoveryNotificationManager* m_discoveryManager;
    AnalyticsNotificationManager* m_analyticsManager;
};

} // namespace ec2
