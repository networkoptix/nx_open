/**********************************************************
* 28 may 2014
* a.kolesnikov
***********************************************************/

#ifndef EC_CONNECTION_NOTIFICATION_MANAGER_H
#define EC_CONNECTION_NOTIFICATION_MANAGER_H

#include "nx_ec/ec_api.h"

#include "transaction/transaction.h"
#include <transaction/transaction_log.h>
#include "transaction/transaction_descriptor.h"

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
    class QnWebPageNotificationManager;
    class QnStoredFileNotificationManager;
    class QnUpdatesNotificationManager;
    class QnMiscNotificationManager;
    class QnDiscoveryNotificationManager;

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
            QnVideowallNotificationManager* videowallManager,
            QnWebPageNotificationManager *webPageManager,
            QnStoredFileNotificationManager* storedFileManager,
            QnUpdatesNotificationManager* updatesManager,
            QnMiscNotificationManager* miscManager,
            QnDiscoveryNotificationManager* discoveryManager);

        template<typename Param>
        struct GenericTransactionDescriptorTriggerVisitor
        {
            template<typename Descriptor>
            void operator ()(const Descriptor &d)
            {
                d.triggerNotificationFunc(m_tran, m_notificationParams);
            }

            GenericTransactionDescriptorTriggerVisitor(const QnTransaction<Param> &tran, const ec2::detail::NotificationParams &notificationParams)
                : m_tran(tran),
                  m_notificationParams(notificationParams)
            {}
        private:
            const QnTransaction<Param> &m_tran;
            const ec2::detail::NotificationParams &m_notificationParams;
        };

        template<typename T>
        void triggerNotification(const QnTransaction<T> &tran)
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
                m_videowallManager,
                m_webPageManager,
                m_storedFileManager,
                m_updatesManager,
                m_miscManager,
                m_discoveryManager
            };

            auto filteredDescriptors = getTransactionDescriptorsFilteredByTransactionParams<T>();
            static_assert(std::tuple_size<decltype(filteredDescriptors)>::value, "Should be at least one Transaction descriptor to proceed");
            GenericTransactionDescriptorTriggerVisitor<T> visitor(tran, notificationParams);
            visitTransactionDescriptorIfValue(tran.command, visitor, filteredDescriptors);
        }

        void triggerNotification( const QnTransaction<ApiLicenseDataList>& tran );
        void triggerNotification( const QnTransaction<ApiLicenseData>& tran );
        void triggerNotification( const QnTransaction<ApiResetBusinessRuleData>& tran );
        void triggerNotification( const QnTransaction<ApiCameraData>& tran );
        void triggerNotification( const QnTransaction<ApiCameraDataList>& tran );
        void triggerNotification( const QnTransaction<ApiCameraAttributesData>& tran );
        void triggerNotification( const QnTransaction<ApiCameraAttributesDataList>& tran );
        void triggerNotification( const QnTransaction<ApiBusinessActionData>& tran );
        void triggerNotification( const QnTransaction<ApiVideowallData>& tran );
        void triggerNotification( const QnTransaction<ApiWebPageData>& tran );
        void triggerNotification( const QnTransaction<ApiIdData>& tran );
        void triggerNotification( const QnTransaction<ApiIdDataList>& tran );
        void triggerNotification( const QnTransaction<ApiMediaServerData>& tran );
        void triggerNotification( const QnTransaction<ApiStorageData>& tran );
        void triggerNotification( const QnTransaction<ApiStorageDataList>& tran );
        void triggerNotification( const QnTransaction<ApiMediaServerUserAttributesData>& tran );
        void triggerNotification( const QnTransaction<ApiMediaServerUserAttributesDataList>& tran );
        void triggerNotification( const QnTransaction<ApiResourceStatusData>& tran );
        void triggerNotification( const QnTransaction<ApiLicenseOverflowData>& tran );
        void triggerNotification( const QnTransaction<ApiResourceParamWithRefData>& tran );
        void triggerNotification( const QnTransaction<ApiResourceParamWithRefDataList>& tran );
        void triggerNotification( const QnTransaction<ApiServerFootageData>& tran );
        void triggerNotification( const QnTransaction<ApiUserData>& tran );
        void triggerNotification( const QnTransaction<ApiAccessRightsData>& tran);
        void triggerNotification( const QnTransaction<ApiBusinessRuleData>& tran );
        void triggerNotification( const QnTransaction<ApiLayoutData>& tran );
        void triggerNotification( const QnTransaction<ApiLayoutDataList>& tran );
        void triggerNotification( const QnTransaction<ApiStoredFileData>& tran );
        void triggerNotification( const QnTransaction<ApiStoredFilePath>& tran );
        void triggerNotification( const QnTransaction<ApiFullInfoData>& tran );
        void triggerNotification( const QnTransaction<ApiResourceParamDataList>& tran );
        void triggerNotification( const QnTransaction<ApiVideowallControlMessageData>& tran );
        void triggerNotification( const QnTransaction<ApiEmailSettingsData>& /*tran*/ );
        void triggerNotification( const QnTransaction<ApiEmailData>& /*tran*/ );
        void triggerNotification( const QnTransaction<ApiUpdateInstallData>& tran );
        void triggerNotification( const QnTransaction<ApiUpdateUploadData>& tran );
        void triggerNotification( const QnTransaction<ApiUpdateUploadResponceData>& tran );
        void triggerNotification(const QnTransaction<ApiDiscoveredServerData> &tran );
        void triggerNotification(const QnTransaction<ApiDiscoveredServerDataList> &tran );
        void triggerNotification( const QnTransaction<ApiDiscoveryData>& tran );
        void triggerNotification( const QnTransaction<ApiDiscoveryDataList>& tran );
        void triggerNotification( const QnTransaction<ApiDiscoverPeerData>& tran );
        void triggerNotification( const QnTransaction<ApiSystemNameData>& tran );
        void triggerNotification( const QnTransaction<ApiRuntimeData>& tran );
        void triggerNotification( const QnTransaction<ApiPeerSystemTimeData>& tran );
        void triggerNotification( const QnTransaction<ApiPeerSystemTimeDataList>& tran );
        void triggerNotification( const QnTransaction<ApiDatabaseDumpData> & /*tran*/ );
        void triggerNotification( const QnTransaction<ApiReverseConnectionData> & tran );
        void triggerNotification( const QnTransaction<ApiClientInfoData> & /*tran*/ );
        void triggerNotification( const QnTransaction<ApiClientInfoDataList> & /*tran*/ );

        void triggerNotification(const QnTransaction<ApiUpdateSequenceData> &/*tran*/) { /* nothing to do */ }

        void triggerNotification(const QnTransaction<ApiLockData> &/*tran*/) {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a system transaction!"); // we MUSTN'T be here
        }
        void triggerNotification(const QnTransaction<ApiPeerAliveData> &/*tran*/)  {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a system transaction!"); // we MUSTN'T be here
        }
        void triggerNotification(const QnTransaction<ApiSyncRequestData> &/*tran*/)  {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a system transaction!"); // we MUSTN'T be here
        }
        void triggerNotification(const QnTransaction<QnTranStateResponse> &/*tran*/) {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a system transaction!"); // we MUSTN'T be here
        }
        void triggerNotification(const QnTransaction<ApiTranSyncDoneData> &/*tran*/) {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a system transaction!"); // we MUSTN'T be here
        }

        void databaseReplaceRequired();
    private:
        AbstractECConnection* m_ecConnection;
        QnLicenseNotificationManager* m_licenseManager;
        QnResourceNotificationManager* m_resourceManager;
        QnMediaServerNotificationManager* m_mediaServerManager;
        QnCameraNotificationManager* m_cameraManager;
        QnUserNotificationManager* m_userManager;
        QnBusinessEventNotificationManager* m_businessEventManager;
        QnLayoutNotificationManager* m_layoutManager;
        QnVideowallNotificationManager* m_videowallManager;
        QnWebPageNotificationManager *m_webPageManager;
        QnStoredFileNotificationManager* m_storedFileManager;
        QnUpdatesNotificationManager* m_updatesManager;
        QnMiscNotificationManager* m_miscManager;
        QnDiscoveryNotificationManager* m_discoveryManager;
    };

}

#endif  //EC_CONNECTION_NOTIFICATION_MANAGER_H
