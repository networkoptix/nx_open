/**********************************************************
* 28 may 2014
* a.kolesnikov
***********************************************************/

#ifndef EC_CONNECTION_NOTIFICATION_MANAGER_H
#define EC_CONNECTION_NOTIFICATION_MANAGER_H

#include "nx_ec/ec_api.h"

#include "transaction/transaction.h"


namespace ec2
{
    class AbstractECConnection;
    class QnLicenseNotificationManager;
    class QnResourceNotificationManager;
    class QnMediaServerNotificationManager;
    class QnCameraNotificationManager;
    class QnUserNotificationManager;
    class QnBusinessEventNotificationManager;
    class QnLayoutNotificationManager;
    class QnVideowallNotificationManager;
    class QnStoredFileNotificationManager;
    class QnUpdatesNotificationManager;

    //!Stands for emitting API notifications
    class ECConnectionNotificationManager
    {
    public:
        ECConnectionNotificationManager(
            const ResourceContext& resCtx,
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
            QnUpdatesNotificationManager* updatesManager );

        void triggerNotification( const QnTransaction<ApiLicenseDataList>& tran );
        void triggerNotification( const QnTransaction<ApiLicenseData>& tran );
        void triggerNotification( const QnTransaction<ApiResetBusinessRuleData>& tran );
        void triggerNotification( const QnTransaction<ApiCameraData>& tran );
        void triggerNotification( const QnTransaction<ApiCameraDataList>& tran );
        void triggerNotification( const QnTransaction<ApiBusinessActionData>& tran );
        void triggerNotification( const QnTransaction<ApiVideowallData>& tran );
        void triggerNotification( const QnTransaction<ApiIdData>& tran );
        void triggerNotification( const QnTransaction<ApiMediaServerData>& tran );
        void triggerNotification( const QnTransaction<ApiResourceData>& tran );
        void triggerNotification( const QnTransaction<ApiSetResourceStatusData>& tran );
        void triggerNotification( const QnTransaction<ApiResourceParamsData>& tran );
        void triggerNotification( const QnTransaction<ApiCameraServerItemData>& tran );
        void triggerNotification( const QnTransaction<ApiUserData>& tran );
        void triggerNotification( const QnTransaction<ApiBusinessRuleData>& tran );
        void triggerNotification( const QnTransaction<ApiLayoutData>& tran );
        void triggerNotification( const QnTransaction<ApiLayoutDataList>& tran );
        void triggerNotification( const QnTransaction<ApiStoredFileData>& tran );
        void triggerNotification( const QnTransaction<ApiFullInfoData>& tran );
        void triggerNotification( const QnTransaction<ApiPanicModeData>& tran );
        void triggerNotification( const QnTransaction<QString>& tran );
        void triggerNotification( const QnTransaction<ApiResourceParamDataList>& tran );
        void triggerNotification(const QnTransaction<ApiVideowallControlMessageData>& tran );
        void triggerNotification( const QnTransaction<ApiEmailSettingsData>& /*tran*/ );
        void triggerNotification( const QnTransaction<ApiEmailData>& /*tran*/ );
        void triggerNotification(const QnTransaction<ApiUpdateUploadData>& tran );
        void triggerNotification(const QnTransaction<ApiUpdateUploadResponceData>& tran );
        void triggerNotification( const QnTransaction<ApiCameraBookmarkTagDataList>& tran );

    private:
        ResourceContext m_resCtx;
        AbstractECConnection* m_ecConnection;
        QnLicenseNotificationManager* m_licenseManager;
        QnResourceNotificationManager* m_resourceManager;
        QnMediaServerNotificationManager* m_mediaServerManager;
        QnCameraNotificationManager* m_cameraManager;
        QnUserNotificationManager* m_userManager;
        QnBusinessEventNotificationManager* m_businessEventManager;
        QnLayoutNotificationManager* m_layoutManager;
        QnVideowallNotificationManager* m_videowallManager;
        QnStoredFileNotificationManager* m_storedFileManager;
        QnUpdatesNotificationManager* m_updatesManager;
    };
}

#endif  //EC_CONNECTION_NOTIFICATION_MANAGER_H
