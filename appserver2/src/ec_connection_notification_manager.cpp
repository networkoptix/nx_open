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
#include "managers/updates_manager.h"
#include "managers/time_manager.h"
#include "managers/misc_manager.h"
#include "managers/discovery_manager.h"


namespace ec2
{
    ECConnectionNotificationManager::ECConnectionNotificationManager(const ResourceContext& resCtx,
        AbstractECConnection* ecConnection,
        QnLicenseNotificationManager* licenseManager,
        QnResourceNotificationManager* resourceManager,
        QnMediaServerNotificationManager* mediaServerManager,
        QnCameraNotificationManager* cameraManager,
        QnUserNotificationManager* userManager,
        QnBusinessEventNotificationManager* businessEventManager,
        QnLayoutNotificationManager* layoutManager,
        QnVideowallNotificationManager* videowallManager,
        QnStoredFileNotificationManager* storedFileManager,
        QnUpdatesNotificationManager* updatesManager,
        QnMiscNotificationManager *miscManager,
        QnDiscoveryNotificationManager *discoveryManager)
    :
        m_resCtx( resCtx ),
        m_ecConnection( ecConnection ),
        m_licenseManager( licenseManager ),
        m_resourceManager( resourceManager ),
        m_mediaServerManager( mediaServerManager ),
        m_cameraManager( cameraManager ),
        m_userManager( userManager ),
        m_businessEventManager( businessEventManager ),
        m_layoutManager( layoutManager ),
        m_videowallManager( videowallManager ),
        m_storedFileManager( storedFileManager ),
        m_updatesManager( updatesManager ),
        m_miscManager( miscManager ),
        m_discoveryManager( discoveryManager )
    {
    }

    void ECConnectionNotificationManager::triggerNotification( const QnTransaction<ApiLicenseDataList>& tran ) {
        m_licenseManager->triggerNotification( tran );
    }

    void ECConnectionNotificationManager::triggerNotification( const QnTransaction<ApiLicenseData>& tran ) {
        m_licenseManager->triggerNotification( tran );
    }

    void ECConnectionNotificationManager::triggerNotification( const QnTransaction<ApiResetBusinessRuleData>& tran ) {
        m_businessEventManager->triggerNotification( tran );
    }

    void ECConnectionNotificationManager::triggerNotification( const QnTransaction<ApiCameraData>& tran ) {
        m_cameraManager->triggerNotification( tran );
    }

    void ECConnectionNotificationManager::triggerNotification( const QnTransaction<ApiCameraDataList>& tran ) {
        m_cameraManager->triggerNotification( tran );
    }

    void ECConnectionNotificationManager::triggerNotification( const QnTransaction<ApiCameraAttributesData>& tran ) {
        m_cameraManager->triggerNotification( tran );
    }

    void ECConnectionNotificationManager::triggerNotification( const QnTransaction<ApiCameraAttributesDataList>& tran ) {
        m_cameraManager->triggerNotification( tran );
    }

    void ECConnectionNotificationManager::triggerNotification( const QnTransaction<ApiBusinessActionData>& tran ) {
        m_businessEventManager->triggerNotification( tran );
    }

    void ECConnectionNotificationManager::triggerNotification( const QnTransaction<ApiVideowallData>& tran ) {
        m_videowallManager->triggerNotification( tran );
    }

    void ECConnectionNotificationManager::triggerNotification( const QnTransaction<ApiIdDataList>& tran ) {
        switch( tran.command )
        {
        case ApiCommand::removeStorages:
            return m_mediaServerManager->triggerNotification( tran );
        case ApiCommand::removeResources:
            return m_resourceManager->triggerNotification( tran );
        default:
            assert( false );
        }
    }

    void ECConnectionNotificationManager::triggerNotification( const QnTransaction<ApiIdData>& tran ) {
        switch( tran.command )
        {
        case ApiCommand::removeResource:
            return m_resourceManager->triggerNotification( tran );
        case ApiCommand::removeCamera:
            return m_cameraManager->triggerNotification( tran );
        case ApiCommand::removeMediaServer:
        case ApiCommand::removeStorage:
            return m_mediaServerManager->triggerNotification( tran );
        case ApiCommand::removeUser:
            return m_userManager->triggerNotification( tran );
        case ApiCommand::removeBusinessRule:
            return m_businessEventManager->triggerNotification( tran );
        case ApiCommand::removeLayout:
            return m_layoutManager->triggerNotification( tran );
        case ApiCommand::removeVideowall:
            return m_videowallManager->triggerNotification( tran );
        case ApiCommand::forcePrimaryTimeServer:
            //#ak no notification needed
            break;
        default:
            assert( false );
        }
    }

    void ECConnectionNotificationManager::triggerNotification( const QnTransaction<ApiRuntimeData>& tran )
    {
        Q_ASSERT(tran.command == ApiCommand::runtimeInfoChanged);
        emit m_ecConnection->runtimeInfoChanged(tran.params);
    }

    void ECConnectionNotificationManager::triggerNotification(const QnTransaction<ApiDatabaseDumpData> & /*tran*/)
    {
        emit m_ecConnection->databaseDumped();
    }

    void ECConnectionNotificationManager::triggerNotification( const QnTransaction<ApiMediaServerData>& tran ) {
        m_mediaServerManager->triggerNotification( tran );
    }

    void ECConnectionNotificationManager::triggerNotification( const QnTransaction<ApiStorageData>& tran ) {
        m_mediaServerManager->triggerNotification( tran );
    }

    void ECConnectionNotificationManager::triggerNotification( const QnTransaction<ApiStorageDataList>& tran ) {
        m_mediaServerManager->triggerNotification( tran );
    }

    void ECConnectionNotificationManager::triggerNotification( const QnTransaction<ApiMediaServerUserAttributesData>& tran ) {
        m_mediaServerManager->triggerNotification( tran );
    }

    void ECConnectionNotificationManager::triggerNotification( const QnTransaction<ApiMediaServerUserAttributesDataList>& tran ) {
        m_mediaServerManager->triggerNotification( tran );
    }

    void ECConnectionNotificationManager::triggerNotification( const QnTransaction<ApiResourceData>& tran ) {
        m_resourceManager->triggerNotification( tran );
    }

    void ECConnectionNotificationManager::triggerNotification( const QnTransaction<ApiResourceStatusData>& tran ) {
        m_resourceManager->triggerNotification( tran );
    }

    /*
    void triggerNotification( const QnTransaction<ApiSetResourceDisabledData>& tran ) {
        m_resourceManager->triggerNotification( tran );
    }
    */

    void ECConnectionNotificationManager::triggerNotification( const QnTransaction<ApiResourceParamWithRefData>& tran ) {
        m_resourceManager->triggerNotification( tran );
    }

    void ECConnectionNotificationManager::triggerNotification( const QnTransaction<ApiResourceParamWithRefDataList>& tran ) {
        m_resourceManager->triggerNotification( tran );
    }

    void ECConnectionNotificationManager::triggerNotification( const QnTransaction<ApiCameraServerItemData>& tran ) {
        return m_cameraManager->triggerNotification( tran );
    }

    void ECConnectionNotificationManager::triggerNotification( const QnTransaction<ApiUserData>& tran ) {
        return m_userManager->triggerNotification( tran );
    }

    void ECConnectionNotificationManager::triggerNotification( const QnTransaction<ApiBusinessRuleData>& tran ) {
        return m_businessEventManager->triggerNotification( tran );
    }

    void ECConnectionNotificationManager::triggerNotification( const QnTransaction<ApiLayoutData>& tran ) {
        return m_layoutManager->triggerNotification( tran );
    }

    void ECConnectionNotificationManager::triggerNotification( const QnTransaction<ApiLayoutDataList>& tran ) {
        return m_layoutManager->triggerNotification( tran );
    }

    void ECConnectionNotificationManager::triggerNotification( const QnTransaction<ApiStoredFilePath>& tran ) {
        assert(tran.command == ApiCommand::removeStoredFile);
        m_storedFileManager->triggerNotification(tran);
    }

    void ECConnectionNotificationManager::triggerNotification( const QnTransaction<ApiStoredFileData>& tran ) {
        return m_storedFileManager->triggerNotification( tran );
    }

    void ECConnectionNotificationManager::triggerNotification( const QnTransaction<ApiFullInfoData>& tran ) {
        QnFullResourceData fullResData;
        fromApiToResourceList(tran.params, fullResData, m_resCtx);
        emit m_ecConnection->initNotification(fullResData);
        for(const ApiDiscoveryData& data: tran.params.discoveryData)
            m_discoveryManager->triggerNotification(data);
    }

    void ECConnectionNotificationManager::triggerNotification( const QnTransaction<ApiPanicModeData>& tran ) {
        emit m_ecConnection->panicModeChanged(tran.params.mode);
    }

    void ECConnectionNotificationManager::triggerNotification(const QnTransaction<ApiVideowallControlMessageData>& tran ) {
        return m_videowallManager->triggerNotification(tran);
    }

    void ECConnectionNotificationManager::triggerNotification( const QnTransaction<ApiEmailSettingsData>& /*tran*/ ) {
    }

    void ECConnectionNotificationManager::triggerNotification( const QnTransaction<ApiEmailData>& /*tran*/ ) {
    }

    void ECConnectionNotificationManager::triggerNotification( const QnTransaction<ApiUpdateInstallData>& tran ) {
        assert(tran.command == ApiCommand::installUpdate);
        m_updatesManager->triggerNotification(tran);
    }

    void ECConnectionNotificationManager::triggerNotification(const QnTransaction<ApiUpdateUploadData>& tran ) {
        m_updatesManager->triggerNotification(tran);
    }

    void ECConnectionNotificationManager::triggerNotification(const QnTransaction<ApiUpdateUploadResponceData>& tran ) {
        m_updatesManager->triggerNotification(tran);
    }

    void ECConnectionNotificationManager::triggerNotification( const QnTransaction<ApiCameraBookmarkTagDataList>& tran ) {
        m_cameraManager->triggerNotification(tran);
    }

    void ECConnectionNotificationManager::triggerNotification( const QnTransaction<ApiPeerSystemTimeData>& /*tran*/ ) {
        //#ak no notification needed in this case
    }

    void ECConnectionNotificationManager::triggerNotification( const QnTransaction<ApiPeerSystemTimeDataList>& /*tran*/ ) {
        //#ak no notification needed in this case
    }

    void ECConnectionNotificationManager::triggerNotification(const QnTransaction<ApiModuleData> &tran) {
        m_miscManager->triggerNotification(tran);
    }

    void ECConnectionNotificationManager::triggerNotification(const QnTransaction<ApiModuleDataList> &tran) {
        m_miscManager->triggerNotification(tran);
    }

    void ECConnectionNotificationManager::triggerNotification(const QnTransaction<ApiDiscoveryData> &tran) {
        m_discoveryManager->triggerNotification(tran);
    }

    void ECConnectionNotificationManager::triggerNotification(const QnTransaction<ApiDiscoverPeerData> &tran) {
        m_discoveryManager->triggerNotification(tran);
    }

    void ECConnectionNotificationManager::triggerNotification(const QnTransaction<ApiSystemNameData> &tran) {
        m_miscManager->triggerNotification(tran);
    }
}
