/**********************************************************
* 21 jan 2014
* a.kolesnikov
***********************************************************/

#ifndef BASE_EC2_CONNECTION_H
#define BASE_EC2_CONNECTION_H

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

    // TODO: #2.4 remove Ec2 suffix to avoid ec2::BaseEc2Connection
    template<class QueryProcessorType>
    class BaseEc2Connection
    :
        public AbstractECConnection
    {
    public:
        BaseEc2Connection(QueryProcessorType* queryProcessor);
        virtual ~BaseEc2Connection();

        virtual AbstractResourceManagerPtr getResourceManager() override;
        virtual AbstractMediaServerManagerPtr getMediaServerManager(const Qn::UserAccessData &userAccessData) override;
        virtual AbstractCameraManagerPtr getCameraManager() override;
        virtual AbstractLicenseManagerPtr getLicenseManager() override;
        virtual AbstractBusinessEventManagerPtr getBusinessEventManager() override;
        virtual AbstractUserManagerPtr getUserManager(const Qn::UserAccessData &userAccessData) override;
        virtual AbstractLayoutManagerPtr getLayoutManager(const Qn::UserAccessData &userAccessData) override;
        virtual AbstractVideowallManagerPtr getVideowallManager(const Qn::UserAccessData &userAccessData) override;
        virtual AbstractWebPageManagerPtr getWebPageManager(const Qn::UserAccessData &userAccessData) override;
        virtual AbstractStoredFileManagerPtr getStoredFileManager() override;
        virtual AbstractUpdatesManagerPtr getUpdatesManager(const Qn::UserAccessData &userAccessData) override;
        virtual AbstractMiscManagerPtr getMiscManager(const Qn::UserAccessData &userAccessData) override;
        virtual AbstractDiscoveryManagerPtr getDiscoveryManager(const Qn::UserAccessData &userAccessData) override;
        virtual AbstractTimeManagerPtr getTimeManager() override;

        virtual void startReceivingNotifications() override;
        virtual void stopReceivingNotifications() override;

        virtual int dumpDatabaseAsync( impl::DumpDatabaseHandlerPtr handler ) override;
        virtual int dumpDatabaseToFileAsync( const QString& dumpFilePath, impl::SimpleHandlerPtr) override;
        virtual int restoreDatabaseAsync( const ec2::ApiDatabaseDumpData& data, impl::SimpleHandlerPtr handler ) override;

        virtual void addRemotePeer(const QUrl& url) override;
        virtual void deleteRemotePeer(const QUrl& url) override;
        virtual void sendRuntimeData(const ec2::ApiRuntimeData &data) override;

        virtual qint64 getTransactionLogTime() const override;
        virtual void setTransactionLogTime(qint64 value) override;

        QueryProcessorType* queryProcessor() const { return m_queryProcessor; }
        ECConnectionNotificationManager* notificationManager() { return m_notificationManager.get(); }
        ECConnectionAuditManager* auditManager() { return m_auditManager.get(); }

        virtual QnUuid routeToPeerVia(const QnUuid& dstPeer, int* distance) const override;
    protected:
        QueryProcessorType* m_queryProcessor;
        std::shared_ptr<QnLicenseManager<QueryProcessorType>> m_licenseManager;
        std::shared_ptr<QnResourceManager<QueryProcessorType>> m_resourceManager;
        QnMediaServerNotificationManagerPtr m_mediaServerManagerBase;
        std::shared_ptr<QnCameraManager<QueryProcessorType>> m_cameraManager;
        QnUserNotificationManagerPtr m_userManagerBase;
        std::shared_ptr<QnBusinessEventManager<QueryProcessorType>> m_businessEventManager;
        QnLayoutNotificationManagerPtr m_layoutManagerBase;
        QnVideowallNotificationManagerPtr m_videowallManagerBase;
        QnWebPageNotificationManagerPtr m_webPageManagerBase;
        std::shared_ptr<QnStoredFileManager<QueryProcessorType>> m_storedFileManager;
        QnUpdatesNotificationManagerPtr m_updatesManagerBase;
        QnMiscNotificationManagerPtr m_miscManagerBase;
        QnDiscoveryNotificationManagerPtr m_discoveryManagerBase;
        std::shared_ptr<QnTimeManager<QueryProcessorType>> m_timeManager;
        std::unique_ptr<ECConnectionNotificationManager> m_notificationManager;
        std::unique_ptr<ECConnectionAuditManager> m_auditManager;
    };
}

#endif  //BASE_EC2_CONNECTION_H
