/**********************************************************
* 21 jan 2014
* a.kolesnikov
***********************************************************/

#ifndef EC2_CONNECTION_H
#define EC2_CONNECTION_H

#include <memory>

#include "camera_manager.h"
#include "media_server_manager.h"
#include "nx_ec/ec_api.h"
#include "resource_manager.h"


namespace ec2
{
    class Ec2DirectConnection
    :
        public AbstractECConnection
    {
    public:
        Ec2DirectConnection();

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
        std::shared_ptr<QnResourceManager> m_resourceManager;
        std::shared_ptr<QnMediaServerManager> m_mediaServerManager;
        std::shared_ptr<QnCameraManager> m_cameraManager;
    };
}

#endif  //EC2_CONNECTION_H
