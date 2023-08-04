// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <managers/analytics_notification_manager.h>
#include <managers/camera_notification_manager.h>
#include <managers/discovery_notification_manager.h>
#include <managers/event_rules_notification_manager.h>
#include <managers/layout_notification_manager.h>
#include <managers/license_notification_manager.h>
#include <managers/lookup_list_notification_manager.h>
#include <managers/media_server_notification_manager.h>
#include <managers/misc_notification_manager.h>
#include <managers/resource_notification_manager.h>
#include <managers/showreel_notification_manager.h>
#include <managers/stored_file_notification_manager.h>
#include <managers/time_notification_manager.h>
#include <managers/user_notification_manager.h>
#include <managers/videowall_notification_manager.h>
#include <managers/vms_rules_notification_manager.h>
#include <managers/webpage_notification_manager.h>
#include <nx/utils/scope_guard.h>
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
        VmsRulesNotificationManager* vmsRulesManager,
        QnLayoutNotificationManager* layoutManager,
        ShowreelNotificationManager* showreelManager,
        QnVideowallNotificationManager* videowallManager,
        QnWebPageNotificationManager* webPageManager,
        QnStoredFileNotificationManager* storedFileManager,
        QnMiscNotificationManager* miscManager,
        QnDiscoveryNotificationManager* discoveryManager,
        AnalyticsNotificationManager* analyticsManager,
        LookupListNotificationManager* lookupListManager);

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
            m_vmsRulesManager,
            m_layoutManager,
            m_showreelManager,
            m_videowallManager,
            m_webPageManager,
            m_storedFileManager,
            m_miscManager,
            m_discoveryManager,
            m_analyticsManager,
            m_lookupListManager,
            source
        };

        auto tdBase = getTransactionDescriptorByValue(tran.command);
        auto td = dynamic_cast<detail::TransactionDescriptor<T>*>(tdBase);
        if (NX_ASSERT(td,
            "Downcast to TransactionDescriptor<TransactionParams>* failed for %1", tran.command))
        {
            td->triggerNotificationFunc(tran, notificationParams);
            callbackMonitors(tran);
        }
    }

    using MonitorCallback = nx::utils::MoveOnlyFunc<void(const QnAbstractTransaction&)>;
    nx::utils::Guard addMonitor(MonitorCallback callback, std::vector<ApiCommand::Value> commands);

private:
    void callbackMonitors(const QnAbstractTransaction& transaction);

private:
    AbstractECConnection* m_ecConnection;
    QnLicenseNotificationManager* m_licenseManager;
    QnResourceNotificationManager* m_resourceManager;
    QnMediaServerNotificationManager* m_mediaServerManager;
    QnCameraNotificationManager* m_cameraManager;
    QnUserNotificationManager* m_userManager;
    QnTimeNotificationManager* m_timeManager;
    QnBusinessEventNotificationManager* m_businessEventManager;
    VmsRulesNotificationManager* m_vmsRulesManager;
    QnLayoutNotificationManager* m_layoutManager;
    ShowreelNotificationManager* m_showreelManager;
    QnVideowallNotificationManager* m_videowallManager;
    QnWebPageNotificationManager* m_webPageManager;
    QnStoredFileNotificationManager* m_storedFileManager;
    QnMiscNotificationManager* m_miscManager;
    QnDiscoveryNotificationManager* m_discoveryManager;
    AnalyticsNotificationManager* m_analyticsManager;
    LookupListNotificationManager* m_lookupListManager;

    nx::Mutex m_mutex;
    std::unordered_map<ApiCommand::Value, std::set<MonitorCallback*>> m_monitoredCommands;
    std::unordered_map<std::shared_ptr<MonitorCallback>, std::vector<ApiCommand::Value>>
        m_monitors;
};

} // namespace ec2
