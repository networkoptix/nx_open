/**********************************************************
* 28 may 2014
* a.kolesnikov
***********************************************************/

#ifndef EC_CONNECTION_NOTIFICATION_MANAGER_H
#define EC_CONNECTION_NOTIFICATION_MANAGER_H

#include "nx_ec/ec_api.h"

#include "transaction/transaction.h"
#include <transaction/transaction_log.h>

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
    class QnMiscNotificationManager;
    class QnDiscoveryNotificationManager;

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
            QnUpdatesNotificationManager* updatesManager,
            QnMiscNotificationManager* miscManager,
            QnDiscoveryNotificationManager* discoveryManager);

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
        void triggerNotification( const QnTransaction<ApiStoredFilePath>& tran );
        void triggerNotification( const QnTransaction<ApiFullInfoData>& tran );
        void triggerNotification( const QnTransaction<ApiPanicModeData>& tran );
        void triggerNotification( const QnTransaction<ApiResourceParamDataList>& tran );
        void triggerNotification( const QnTransaction<ApiVideowallControlMessageData>& tran );
        void triggerNotification( const QnTransaction<ApiEmailSettingsData>& /*tran*/ );
        void triggerNotification( const QnTransaction<ApiEmailData>& /*tran*/ );
        void triggerNotification( const QnTransaction<ApiUpdateInstallData>& tran );
        void triggerNotification( const QnTransaction<ApiUpdateUploadData>& tran );
        void triggerNotification( const QnTransaction<ApiUpdateUploadResponceData>& tran );
        void triggerNotification( const QnTransaction<ApiCameraBookmarkTagDataList>& tran );
        void triggerNotification( const QnTransaction<ApiModuleData>& tran );
        void triggerNotification( const QnTransaction<ApiModuleDataList>& tran );
        void triggerNotification( const QnTransaction<ApiDiscoveryDataList>& tran );
        void triggerNotification( const QnTransaction<ApiDiscoverPeerData>& tran );
        void triggerNotification( const QnTransaction<ApiConnectionData>& tran );
        void triggerNotification( const QnTransaction<ApiConnectionDataList>& tran );
        void triggerNotification( const QnTransaction<ApiSystemNameData>& tran );
        void triggerNotification( const QnTransaction<ApiRuntimeData>& tran );
        void triggerNotification( const QnTransaction<ApiPeerSystemTimeData>& tran );
        void triggerNotification( const QnTransaction<ApiDatabaseDumpData> & /*tran*/ );

        void triggerNotification(const QnTransaction<ApiFillerData> &/*tran*/) { /* nothing to do */ }

        void triggerNotification(const QnTransaction<ApiLockData> &/*tran*/) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a system transaction!"); // we MUSTN'T be here
        }
        void triggerNotification(const QnTransaction<ApiPeerAliveData> &/*tran*/)  {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a system transaction!"); // we MUSTN'T be here
        }
        void triggerNotification(const QnTransaction<QnTranState> &/*tran*/)  {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a system transaction!"); // we MUSTN'T be here
        }
        void triggerNotification(const QnTransaction<QnTranStateResponse> &/*tran*/) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a system transaction!"); // we MUSTN'T be here
        }
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
        QnMiscNotificationManager* m_miscManager;
        QnDiscoveryNotificationManager* m_discoveryManager;
    };
}

#endif  //EC_CONNECTION_NOTIFICATION_MANAGER_H
