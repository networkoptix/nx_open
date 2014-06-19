/**********************************************************
* 21 jan 2014
* a.kolesnikov
***********************************************************/

#ifndef BASE_EC2_CONNECTION_H
#define BASE_EC2_CONNECTION_H

#include <memory>

#include "nx_ec/ec_api.h"
#include "core/resource_management/resource_pool.h"
#include "nx_ec/data/api_media_server_data.h"
#include "transaction/transaction.h"
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
#include "managers/misc_manager.h"
#include "managers/discovery_manager.h"

#include "nx_ec/data/api_full_info_data.h"
#include "nx_ec/data/api_videowall_data.h"
#include "nx_ec/data/api_conversion_functions.h"


namespace ec2
{
    template<class QueryProcessorType>
    class BaseEc2Connection
    :
        public AbstractECConnection
    {
    public:
        BaseEc2Connection(
            QueryProcessorType* queryProcessor,
            const ResourceContext& resCtx );

        virtual AbstractResourceManagerPtr getResourceManager() override;
        virtual AbstractMediaServerManagerPtr getMediaServerManager() override;
        virtual AbstractCameraManagerPtr getCameraManager() override;
        virtual AbstractLicenseManagerPtr getLicenseManager() override;
        virtual AbstractBusinessEventManagerPtr getBusinessEventManager() override;
        virtual AbstractUserManagerPtr getUserManager() override;
        virtual AbstractLayoutManagerPtr getLayoutManager() override;
        virtual AbstractVideowallManagerPtr getVideowallManager() override;
        virtual AbstractStoredFileManagerPtr getStoredFileManager() override;
        virtual AbstractUpdatesManagerPtr getUpdatesManager() override;
        virtual AbstractMiscManagerPtr getMiscManager() override;
        virtual AbstractDiscoveryManagerPtr getDiscoveryManager() override;

        virtual int setPanicMode( Qn::PanicMode value, impl::SimpleHandlerPtr handler ) override;
        virtual int getCurrentTime( impl::CurrentTimeHandlerPtr handler ) override;
        virtual int dumpDatabaseAsync( impl::DumpDatabaseHandlerPtr handler ) override;
        virtual int restoreDatabaseAsync( const QByteArray& dbFile, impl::SimpleHandlerPtr handler ) override;
        virtual int getSettingsAsync( impl::GetSettingsHandlerPtr handler ) override;
        virtual int saveSettingsAsync( const QnKvPairList& kvPairs, impl::SimpleHandlerPtr handler ) override;

        virtual void addRemotePeer(const QUrl& url, const QUuid& peerGuid) override;
        virtual void deleteRemotePeer(const QUrl& url) override;
    public:

        template<class T> bool processIncomingTransaction( const QnTransaction<T>& tran, const QByteArray& serializedTran) {
            if (!m_queryProcessor->processIncomingTransaction(tran, serializedTran))
                return false;
            triggerNotification(tran);
            return true;
        }

        void triggerNotification( const QnTransaction<ApiLicenseDataList>& tran ) {
            m_licenseManager->triggerNotification( tran );
        }

        void triggerNotification( const QnTransaction<ApiLicenseData>& tran ) {
            m_licenseManager->triggerNotification( tran );
        }

        void triggerNotification( const QnTransaction<ApiResetBusinessRuleData>& tran ) {
            m_businessEventManager->triggerNotification( tran );
        }

        void triggerNotification( const QnTransaction<ApiCameraData>& tran ) {
            m_cameraManager->triggerNotification( tran );
        }

        void triggerNotification( const QnTransaction<ApiCameraDataList>& tran ) {
            m_cameraManager->triggerNotification( tran );
        }

        void triggerNotification( const QnTransaction<ApiBusinessActionData>& tran ) {
            m_businessEventManager->triggerNotification( tran );
        }

        void triggerNotification( const QnTransaction<ApiVideowallData>& tran ) {
            m_videowallManager->triggerNotification( tran );
        }

        void triggerNotification( const QnTransaction<ApiIdData>& tran ) {
            switch( tran.command )
            {
            case ApiCommand::removeResource:
                return m_resourceManager->triggerNotification( tran );
            case ApiCommand::removeCamera:
                return m_cameraManager->triggerNotification( tran );
            case ApiCommand::removeMediaServer:
                return m_mediaServerManager->triggerNotification( tran );
            case ApiCommand::removeUser:
                return m_userManager->triggerNotification( tran );
            case ApiCommand::removeBusinessRule:
                return m_businessEventManager->triggerNotification( tran );
            case ApiCommand::removeLayout:
                return m_layoutManager->triggerNotification( tran );
            case ApiCommand::removeVideowall:
                return m_videowallManager->triggerNotification( tran );
            default:
                assert( false );
            }
        }

        void triggerNotification( const QnTransaction<ApiMediaServerData>& tran ) {
            m_mediaServerManager->triggerNotification( tran );
        }

        void triggerNotification( const QnTransaction<ApiResourceData>& tran ) {
            m_resourceManager->triggerNotification( tran );
        }

        void triggerNotification( const QnTransaction<ApiSetResourceStatusData>& tran ) {
            m_resourceManager->triggerNotification( tran );
        }

        /*
        void triggerNotification( const QnTransaction<ApiSetResourceDisabledData>& tran ) {
            m_resourceManager->triggerNotification( tran );
        }
        */

        void triggerNotification( const QnTransaction<ApiResourceParamsData>& tran ) {
            m_resourceManager->triggerNotification( tran );
        }

        void triggerNotification( const QnTransaction<ApiCameraServerItemData>& tran ) {
            return m_cameraManager->triggerNotification( tran );
        }

        void triggerNotification( const QnTransaction<ApiUserData>& tran ) {
            return m_userManager->triggerNotification( tran );
        }

        void triggerNotification( const QnTransaction<ApiBusinessRuleData>& tran ) {
            return m_businessEventManager->triggerNotification( tran );
        }

        void triggerNotification( const QnTransaction<ApiLayoutData>& tran ) {
            return m_layoutManager->triggerNotification( tran );
        }

        void triggerNotification( const QnTransaction<ApiLayoutDataList>& tran ) {
            return m_layoutManager->triggerNotification( tran );
        }

        void triggerNotification( const QnTransaction<ApiStoredFileData>& tran ) {
            return m_storedFileManager->triggerNotification( tran );
        }

        void triggerNotification( const QnTransaction<ApiFullInfoData>& tran ) {
            QnFullResourceData fullResData;
            fromApiToResourceList(tran.params, fullResData, m_resCtx);
            emit initNotification(fullResData);
            m_miscManager->triggerNotification(tran.params.foundModules);
            m_discoveryManager->triggerNotification(tran.params.discoveryData);
        }

        void triggerNotification( const QnTransaction<ApiPanicModeData>& tran ) {
            emit panicModeChanged(tran.params.mode);
        }

        void triggerNotification( const QnTransaction<QString>& tran ) {
            switch (tran.command) {
            case ApiCommand::removeStoredFile:
                m_storedFileManager->triggerNotification(tran);
                break;
            case ApiCommand::installUpdate:
                m_updatesManager->triggerNotification(tran);
                break;
            case ApiCommand::changeSystemName:
                m_miscManager->triggerNotification(tran);
            default:
                assert(false); // we should never get here
            }
        }

        void triggerNotification( const QnTransaction<ApiResourceParamDataList>& tran ) {
            if( tran.command == ApiCommand::saveSettings ) {
                QnKvPairList newSettings;
                fromApiToResourceList(tran.params, newSettings);
                emit settingsChanged(newSettings);
            }
        }

        void triggerNotification(const QnTransaction<ApiVideowallControlMessageData> &tran) {
            return m_videowallManager->triggerNotification(tran);
        }

        void triggerNotification(const QnTransaction<ApiVideowallInstanceStatusData> &tran) {
            return m_videowallManager->triggerNotification(tran);
        }

        void triggerNotification( const QnTransaction<ApiEmailSettingsData>&  ) {
        }

        void triggerNotification( const QnTransaction<ApiEmailData>&  ) {
        }

        void triggerNotification(const QnTransaction<ApiUpdateUploadData> &tran) {
            m_updatesManager->triggerNotification(tran);
        }

        void triggerNotification(const QnTransaction<ApiUpdateUploadResponceData> &tran) {
            m_updatesManager->triggerNotification(tran);
        }

        void triggerNotification( const QnTransaction<ApiCameraBookmarkTagDataList> &tran) {
            m_cameraManager->triggerNotification(tran);
        }

        void triggerNotification(const QnTransaction<ApiModuleData> &tran) {
            m_miscManager->triggerNotification(tran);
        }

        void triggerNotification(const QnTransaction<ApiDiscoveryDataList> &tran) {
            m_discoveryManager->triggerNotification(tran);
        }

        void triggerNotification(const QnTransaction<ApiDiscoverPeerData> &tran) {
            m_discoveryManager->triggerNotification(tran);
        }

        QueryProcessorType* queryProcessor() const { return m_queryProcessor; }
    protected:
        QueryProcessorType* m_queryProcessor;
        ResourceContext m_resCtx;
        std::shared_ptr<QnLicenseManager<QueryProcessorType>> m_licenseManager;
        std::shared_ptr<QnResourceManager<QueryProcessorType>> m_resourceManager;
        std::shared_ptr<QnMediaServerManager<QueryProcessorType>> m_mediaServerManager;
        std::shared_ptr<QnCameraManager<QueryProcessorType>> m_cameraManager;
        std::shared_ptr<QnUserManager<QueryProcessorType>> m_userManager;
        std::shared_ptr<QnBusinessEventManager<QueryProcessorType>> m_businessEventManager;
        std::shared_ptr<QnLayoutManager<QueryProcessorType>> m_layoutManager;
        std::shared_ptr<QnVideowallManager<QueryProcessorType>> m_videowallManager;
        std::shared_ptr<QnStoredFileManager<QueryProcessorType>> m_storedFileManager;
        std::shared_ptr<QnUpdatesManager<QueryProcessorType>> m_updatesManager;
        std::shared_ptr<QnMiscManager<QueryProcessorType>> m_miscManager;
        std::shared_ptr<QnDiscoveryManager<QueryProcessorType>> m_discoveryManager;

    private:
        QnTransaction<ApiPanicModeData> prepareTransaction( ApiCommand::Value cmd, const Qn::PanicMode& mode);
    };
}

#endif  //BASE_EC2_CONNECTION_H
