/**********************************************************
* 21 jan 2014
* a.kolesnikov
***********************************************************/

#ifndef BASE_EC2_CONNECTION_H
#define BASE_EC2_CONNECTION_H

#include <memory>

#include "core/resource_management/resource_pool.h"
#include "ec_connection_notification_manager.h"
#include "nx_ec/data/api_media_server_data.h"
#include "nx_ec/data/api_full_info_data.h"
#include "nx_ec/data/api_videowall_data.h"
#include "nx_ec/data/api_conversion_functions.h"

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


namespace ec2
{
    class ECConnectionNotificationManager;

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

        virtual int setPanicMode( Qn::PanicMode value, impl::SimpleHandlerPtr handler ) override;
        virtual int getCurrentTime( impl::CurrentTimeHandlerPtr handler ) override;
        virtual int dumpDatabaseAsync( impl::DumpDatabaseHandlerPtr handler ) override;
        virtual int restoreDatabaseAsync( const QByteArray& dbFile, impl::SimpleHandlerPtr handler ) override;
        virtual int getSettingsAsync( impl::GetSettingsHandlerPtr handler ) override;
        virtual int saveSettingsAsync( const QnKvPairList& kvPairs, impl::SimpleHandlerPtr handler ) override;

        virtual void addRemotePeer(const QUrl& url, const QUuid& peerGuid) override;
        virtual void deleteRemotePeer(const QUrl& url) override;
        virtual void sendRuntimeData(const ec2::ApiRuntimeData &data) override;

        QueryProcessorType* queryProcessor() const { return m_queryProcessor; }
        ECConnectionNotificationManager* notificationManager() { return m_notificationManager.get(); }

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
        std::unique_ptr<ECConnectionNotificationManager> m_notificationManager;

    private:
        QnTransaction<ApiPanicModeData> prepareTransaction( ApiCommand::Value cmd, const Qn::PanicMode& mode);
    };
}

#endif  //BASE_EC2_CONNECTION_H
