/**********************************************************
* 21 jan 2014
* a.kolesnikov
***********************************************************/

#ifndef BASE_EC2_CONNECTION_H
#define BASE_EC2_CONNECTION_H

#include <memory>

#include "camera_manager.h"
#include "media_server_manager.h"
#include "nx_ec/ec_api.h"
#include "resource_manager.h"


namespace ec2
{
    template<class QueryProcessorType>
    class BaseEc2Connection
    :
        public AbstractECConnection
    {
    public:
        BaseEc2Connection( QueryProcessorType* queryProcessor, const QnResourceFactoryPtr& resourceFactory );

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
        AbstractResourceManagerPtr m_resourceManager;
        AbstractMediaServerManagerPtr m_mediaServerManager;
        AbstractCameraManagerPtr m_cameraManager;
        AbstractUserManagerPtr m_userManager;
    };
}

#endif  //BASE_EC2_CONNECTION_H
