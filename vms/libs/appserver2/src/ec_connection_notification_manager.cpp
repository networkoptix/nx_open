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
    QnLayoutTourNotificationManager* showreelManager,
    QnVideowallNotificationManager* videowallManager,
    QnWebPageNotificationManager* webPageManager,
    QnStoredFileNotificationManager* storedFileManager,
    QnMiscNotificationManager* miscManager,
    QnDiscoveryNotificationManager* discoveryManager,
    AnalyticsNotificationManager* analyticsManager)
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
    m_analyticsManager(analyticsManager)
{
}

} // namespace ec2
