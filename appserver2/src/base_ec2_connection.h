/**********************************************************
* 21 jan 2014
* a.kolesnikov
***********************************************************/

#ifndef BASE_EC2_CONNECTION_H
#define BASE_EC2_CONNECTION_H

#include <memory>

#include "nx_ec/ec_api.h"
#include "core/resource_management/resource_pool.h"
#include "nx_ec/data/mserver_data.h"
#include "transaction/transaction.h"
#include "business_event_manager.h"
#include "camera_manager.h"
#include "managers/layout_manager.h"
#include "managers/license_manager.h"
#include "managers/stored_file_manager.h"
#include "media_server_manager.h"
#include "resource_manager.h"
#include "user_manager.h"


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
        virtual AbstractStoredFileManagerPtr getStoredFileManager() override;

        virtual int setPanicMode( Qn::PanicMode value, impl::SimpleHandlerPtr handler ) override;
        virtual int getCurrentTime( impl::CurrentTimeHandlerPtr handler ) override;
        virtual int dumpDatabaseAsync( impl::DumpDatabaseHandlerPtr handler ) override;
        virtual int restoreDatabaseAsync( const QByteArray& dbFile, impl::SimpleHandlerPtr handler ) override;
        virtual int getSettingsAsync( impl::GetSettingsHandlerPtr handler ) override;
        virtual int saveSettingsAsync( const QnKvPairList& kvPairs, impl::SimpleHandlerPtr handler ) override;

    protected:
        QueryProcessorType* m_queryProcessor;
        std::shared_ptr<QnLicenseManager<QueryProcessorType>> m_licenseManager;
        std::shared_ptr<QnResourceManager<QueryProcessorType>> m_resourceManager;
        std::shared_ptr<QnMediaServerManager<QueryProcessorType>> m_mediaServerManager;
        std::shared_ptr<QnCameraManager<QueryProcessorType>> m_cameraManager;
        std::shared_ptr<QnUserManager<QueryProcessorType>> m_userManager;
        std::shared_ptr<QnBusinessEventManager<QueryProcessorType>> m_businessEventManager;
        std::shared_ptr<QnLayoutManager<QueryProcessorType>> m_layoutManager;
        std::shared_ptr<QnStoredFileManager<QueryProcessorType>> m_storedFileManager;

    private:
        QnTransaction<ApiPanicModeData> prepareTransaction( ApiCommand::Value cmd, const Qn::PanicMode& mode);
    };
}

#endif  //BASE_EC2_CONNECTION_H
