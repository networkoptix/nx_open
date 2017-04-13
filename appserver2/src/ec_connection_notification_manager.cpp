#include "ec_connection_notification_manager.h"

namespace ec2 {

ECConnectionNotificationManager::ECConnectionNotificationManager(
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
    QnDiscoveryNotificationManager* discoveryManager)
    :
    m_ecConnection(ecConnection),
    m_licenseManager(licenseManager),
    m_resourceManager(resourceManager),
    m_mediaServerManager(mediaServerManager),
    m_cameraManager(cameraManager),
    m_userManager(userManager),
    m_businessEventManager(businessEventManager),
    m_layoutManager(layoutManager),
    m_layoutTourManager(layoutTourManager),
    m_videowallManager(videowallManager),
    m_webPageManager(webPageManager),
    m_storedFileManager(storedFileManager),
    m_updatesManager(updatesManager),
    m_miscManager(miscManager),
    m_discoveryManager(discoveryManager)
{
}

} // namespace ec2
