/**********************************************************
* 21 jan 2014
* a.kolesnikov
***********************************************************/

#ifndef BASE_EC2_CONNECTION_H
#define BASE_EC2_CONNECTION_H

#include <memory>

#include "nx_ec/ec_api.h"
#include "core/resource_management/resource_pool.h"


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

        virtual ReqID setPanicMode( PanicMode value, impl::SimpleHandlerPtr handler ) override;
        virtual ReqID getCurrentTime( impl::CurrentTimeHandlerPtr handler ) override;
        virtual ReqID dumpDatabaseAsync( impl::DumpDatabaseHandlerPtr handler ) override;
        virtual ReqID restoreDatabaseAsync( const QByteArray& dbFile, impl::SimpleHandlerPtr handler ) override;
        virtual ReqID getSettingsAsync( impl::GetSettingsHandlerPtr handler ) override;
        virtual ReqID saveSettingsAsync( const QnKvPairList& kvPairs, impl::SimpleHandlerPtr handler ) override;

    private:
        QueryProcessorType* m_queryProcessor;
        AbstractLicenseManagerPtr m_licenseManager;
        AbstractResourceManagerPtr m_resourceManager;
        AbstractMediaServerManagerPtr m_mediaServerManager;
        AbstractCameraManagerPtr m_cameraManager;
        AbstractUserManagerPtr m_userManager;
        AbstractBusinessEventManagerPtr m_businessEventManager;
    };
}

#endif  //BASE_EC2_CONNECTION_H
