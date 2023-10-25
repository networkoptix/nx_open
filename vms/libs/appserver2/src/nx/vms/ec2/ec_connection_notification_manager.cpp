// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ec_connection_notification_manager.h"

namespace ec2 {

ECConnectionNotificationManager::ECConnectionNotificationManager(
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
    LookupListNotificationManager* lookupListManager)
    :
    m_ecConnection(ecConnection),
    m_licenseManager(licenseManager),
    m_resourceManager(resourceManager),
    m_mediaServerManager(mediaServerManager),
    m_cameraManager(cameraManager),
    m_userManager(userManager),
    m_timeManager(timeManager),
    m_businessEventManager(businessEventManager),
    m_vmsRulesManager(vmsRulesManager),
    m_layoutManager(layoutManager),
    m_showreelManager(showreelManager),
    m_videowallManager(videowallManager),
    m_webPageManager(webPageManager),
    m_storedFileManager(storedFileManager),
    m_miscManager(miscManager),
    m_discoveryManager(discoveryManager),
    m_analyticsManager(analyticsManager),
    m_lookupListManager(lookupListManager)
{
}

nx::utils::Guard ECConnectionNotificationManager::addMonitor(
    MonitorCallback callback, std::vector<ApiCommand::Value> commands)
{
    auto monitor = std::make_shared<MonitorCallback>(std::move(callback));
    nx::utils::Guard guard{
        [this, monitor]()
        {
            NX_MUTEX_LOCKER lock(&m_mutex);
            auto commands = m_monitors.find(monitor);
            if (commands != m_monitors.end())
            {
                for (auto c: commands->second)
                {
                    if (auto it = m_monitoredCommands.find(c); it != m_monitoredCommands.end())
                    {
                        it->second.erase(monitor.get());
                        if (it->second.empty())
                            m_monitoredCommands.erase(c);
                    }
                }
                m_monitors.erase(monitor);
            }
        }};
    NX_MUTEX_LOCKER lock(&m_mutex);
    for (auto c: commands)
        m_monitoredCommands[c].insert(monitor.get());
    m_monitors.emplace(std::move(monitor), std::move(commands));
    return guard;
}

void ECConnectionNotificationManager::callbackMonitors(const QnAbstractTransaction& transaction)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (auto monitors = m_monitoredCommands.find(transaction.command);
        monitors != m_monitoredCommands.end())
    {
        for (const auto& callback: monitors->second)
            (*callback)(transaction);
    }
}

} // namespace ec2
