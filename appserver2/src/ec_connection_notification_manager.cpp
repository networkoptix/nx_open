/**********************************************************
* 28 may 2014
* a.kolesnikov
***********************************************************/

#include "ec_connection_notification_manager.h"

#include "managers/business_event_manager.h"
#include "managers/camera_manager.h"
#include "managers/layout_manager.h"
#include "managers/license_manager.h"
#include "managers/stored_file_manager.h"
#include "managers/media_server_manager.h"
#include "managers/resource_manager.h"
#include "managers/user_manager.h"
#include "managers/videowall_manager.h"
#include "managers/webpage_manager.h"
#include "managers/updates_manager.h"
#include "managers/time_manager.h"
#include "managers/misc_manager.h"
#include "managers/discovery_manager.h"

namespace ec2
{
    ECConnectionNotificationManager::ECConnectionNotificationManager(
        AbstractECConnection* ecConnection,
        QnLicenseNotificationManager* licenseManager,
        QnResourceNotificationManager* resourceManager,
        QnMediaServerNotificationManager* mediaServerManager,
        QnCameraNotificationManager* cameraManager,
        QnUserNotificationManager* userManager,
        QnBusinessEventNotificationManager* businessEventManager,
        QnLayoutNotificationManager* layoutManager,
        QnVideowallNotificationManager* videowallManager,
        QnWebPageNotificationManager *webPageManager,
        QnStoredFileNotificationManager* storedFileManager,
        QnUpdatesNotificationManager* updatesManager,
        QnMiscNotificationManager *miscManager,
        QnDiscoveryNotificationManager *discoveryManager)
    :
        m_ecConnection( ecConnection ),
        m_licenseManager( licenseManager ),
        m_resourceManager( resourceManager ),
        m_mediaServerManager( mediaServerManager ),
        m_cameraManager( cameraManager ),
        m_userManager( userManager ),
        m_businessEventManager( businessEventManager ),
        m_layoutManager( layoutManager ),
        m_videowallManager( videowallManager ),
        m_webPageManager (webPageManager),
        m_storedFileManager( storedFileManager ),
        m_updatesManager( updatesManager ),
        m_miscManager( miscManager ),
        m_discoveryManager( discoveryManager )
    {
    }
}
