#pragma once

#include <memory>

#include "core/resource_management/resource_pool.h"
#include "ec_connection_notification_manager.h"
#include "ec_connection_audit_manager.h"
#include "nx_ec/data/api_media_server_data.h"
#include "nx_ec/data/api_full_info_data.h"
#include "nx_ec/data/api_videowall_data.h"
#include "nx_ec/data/api_conversion_functions.h"

#include "managers/business_event_manager.h"
#include "managers/camera_manager.h"
#include "managers/layout_manager.h"
#include <managers/layout_tour_manager.h>
#include "managers/license_manager.h"
#include "managers/stored_file_manager.h"
#include "managers/media_server_manager.h"
#include "managers/resource_manager.h"
#include "managers/user_manager.h"
#include "managers/videowall_manager.h"
#include "managers/webpage_manager.h"
#include "managers/updates_manager.h"
#include "managers/misc_manager.h"
#include "managers/discovery_manager.h"
#include "managers/time_manager_api.h"


namespace ec2
{
    class ECConnectionNotificationManager;
    class Ec2DirectConnectionFactory;

    // TODO: #2.4 remove Ec2 suffix to avoid ec2::BaseEc2Connection
    template<class QueryProcessorType>
    class BaseEc2Connection
    :
        public AbstractECConnection
    {
    public:
        BaseEc2Connection(
            const AbstractECConnectionFactory* connectionFactory,
            QueryProcessorType* queryProcessor);
        virtual ~BaseEc2Connection();

        virtual AbstractResourceManagerPtr getResourceManager(const Qn::UserAccessData &userAccessData) override;
        virtual AbstractMediaServerManagerPtr getMediaServerManager(const Qn::UserAccessData &userAccessData) override;
        virtual AbstractCameraManagerPtr getCameraManager(const Qn::UserAccessData &userAccessData) override;
        virtual AbstractLicenseManagerPtr getLicenseManager(const Qn::UserAccessData &userAccessData) override;
        virtual AbstractBusinessEventManagerPtr getBusinessEventManager(const Qn::UserAccessData &userAccessData) override;
        virtual AbstractUserManagerPtr getUserManager(const Qn::UserAccessData &userAccessData) override;
        virtual AbstractLayoutManagerPtr getLayoutManager(const Qn::UserAccessData &userAccessData) override;
        virtual AbstractLayoutTourManagerPtr getLayoutTourManager(const Qn::UserAccessData& userAccessData) override;
        virtual AbstractVideowallManagerPtr getVideowallManager(const Qn::UserAccessData &userAccessData) override;
        virtual AbstractWebPageManagerPtr getWebPageManager(const Qn::UserAccessData &userAccessData) override;
        virtual AbstractStoredFileManagerPtr getStoredFileManager(const Qn::UserAccessData &userAccessData) override;
        virtual AbstractUpdatesManagerPtr getUpdatesManager(const Qn::UserAccessData &userAccessData) override;
        virtual AbstractMiscManagerPtr getMiscManager(const Qn::UserAccessData &userAccessData) override;
        virtual AbstractDiscoveryManagerPtr getDiscoveryManager(const Qn::UserAccessData &userAccessData) override;
        virtual AbstractTimeManagerPtr getTimeManager(const Qn::UserAccessData &userAccessData) override;

        virtual AbstractLicenseNotificationManagerPtr getLicenseNotificationManager() override;
        virtual AbstractTimeNotificationManagerPtr getTimeNotificationManager() override;
        virtual AbstractResourceNotificationManagerPtr getResourceNotificationManager() override;
        virtual AbstractMediaServerNotificationManagerPtr getMediaServerNotificationManager() override;
        virtual AbstractCameraNotificationManagerPtr getCameraNotificationManager() override;
        virtual AbstractBusinessEventNotificationManagerPtr getBusinessEventNotificationManager() override;
        virtual AbstractUserNotificationManagerPtr getUserNotificationManager() override;
        virtual AbstractLayoutNotificationManagerPtr getLayoutNotificationManager() override;
        virtual AbstractLayoutTourNotificationManagerPtr getLayoutTourNotificationManager() override;
        virtual AbstractWebPageNotificationManagerPtr getWebPageNotificationManager() override;
        virtual AbstractDiscoveryNotificationManagerPtr getDiscoveryNotificationManager() override;
        virtual AbstractMiscNotificationManagerPtr getMiscNotificationManager() override;
        virtual AbstractUpdatesNotificationManagerPtr getUpdatesNotificationManager() override;
        virtual AbstractStoredFileNotificationManagerPtr getStoredFileNotificationManager() override;
        virtual AbstractVideowallNotificationManagerPtr getVideowallNotificationManager() override;

        virtual QnCommonModule* commonModule() const override;

        virtual void startReceivingNotifications() override;
        virtual void stopReceivingNotifications() override;

        virtual int dumpDatabaseAsync( impl::DumpDatabaseHandlerPtr handler ) override;
        virtual int dumpDatabaseToFileAsync( const QString& dumpFilePath, impl::SimpleHandlerPtr) override;
        virtual int restoreDatabaseAsync( const ec2::ApiDatabaseDumpData& data, impl::SimpleHandlerPtr handler ) override;

        virtual void addRemotePeer(const QnUuid& id, const nx::utils::Url& url) override;
        virtual void deleteRemotePeer(const QnUuid& id) override;

        QueryProcessorType* queryProcessor() const { return m_queryProcessor; }
        virtual ECConnectionNotificationManager* notificationManager() override;
        ECConnectionAuditManager* auditManager() { return m_auditManager.get(); }

        virtual QnUuid routeToPeerVia(const QnUuid& dstPeer, int* distance) const override;

        virtual TransactionMessageBusAdapter* messageBus() const override;
    protected:
        const AbstractECConnectionFactory* m_connectionFactory;
        QueryProcessorType* m_queryProcessor;
        QnLicenseNotificationManagerPtr m_licenseNotificationManager;
        QnResourceNotificationManagerPtr m_resourceNotificationManager;
        QnMediaServerNotificationManagerPtr m_mediaServerNotificationManager;
        QnCameraNotificationManagerPtr m_cameraNotificationManager;
        QnUserNotificationManagerPtr m_userNotificationManager;
        QnBusinessEventNotificationManagerPtr m_businessEventNotificationManager;
        QnLayoutNotificationManagerPtr m_layoutNotificationManager;
        QnLayoutTourNotificationManagerPtr m_layoutTourNotificationManager;
        QnVideowallNotificationManagerPtr m_videowallNotificationManager;
        QnWebPageNotificationManagerPtr m_webPageNotificationManager;
        QnStoredFileNotificationManagerPtr m_storedFileNotificationManager;
        QnUpdatesNotificationManagerPtr m_updatesNotificationManager;
        QnMiscNotificationManagerPtr m_miscNotificationManager;
        QnDiscoveryNotificationManagerPtr m_discoveryNotificationManager;
        AbstractTimeNotificationManagerPtr m_timeNotificationManager;
        std::unique_ptr<ECConnectionNotificationManager> m_notificationManager;
        std::unique_ptr<ECConnectionAuditManager> m_auditManager;
    };
}
