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
#include "managers/misc_manager.h"
#include "managers/discovery_manager.h"
#include "managers/time_manager_api.h"


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
        virtual ~BaseEc2Connection();

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
        virtual AbstractTimeManagerPtr getTimeManager() override;

        virtual void startReceivingNotifications() override;
        virtual void stopReceivingNotifications() override;

        virtual int dumpDatabaseAsync( impl::DumpDatabaseHandlerPtr handler ) override;
        virtual int dumpDatabaseToFileAsync( const QString& dumpFilePath, impl::SimpleHandlerPtr handler ) override;
        virtual int restoreDatabaseAsync( const ec2::ApiDatabaseDumpData& data, impl::SimpleHandlerPtr handler ) override;

        virtual void addRemotePeer(const QUrl& url) override;
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
        std::shared_ptr<QnMiscManager<QueryProcessorType>> m_miscManager;
        std::shared_ptr<QnDiscoveryManager<QueryProcessorType>> m_discoveryManager;
        std::shared_ptr<QnTimeManager<QueryProcessorType>> m_timeManager;
        std::unique_ptr<ECConnectionNotificationManager> m_notificationManager;
    };
}

#endif  //BASE_EC2_CONNECTION_H
