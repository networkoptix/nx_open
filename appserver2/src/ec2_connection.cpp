/**********************************************************
* 21 jan 2014
* a.kolesnikov
***********************************************************/

#include "ec2_connection.h"


namespace ec2
{
    Ec2DirectConnection::Ec2DirectConnection()
    :
        m_resourceManager( new QnResourceManager<decltype(m_queryProcessor)>(&m_queryProcessor) ),
        m_mediaServerManager( new QnMediaServerManager() ),
        m_cameraManager( new QnCameraManager<decltype(m_queryProcessor)>(&m_queryProcessor) ),
		m_dbManager(new QnDbManager())
    {
    }

    AbstractResourceManagerPtr Ec2DirectConnection::getResourceManager()
    {
        return m_resourceManager;
    }

    AbstractMediaServerManagerPtr Ec2DirectConnection::getMediaServerManager()
    {
        return m_mediaServerManager;
    }

    AbstractCameraManagerPtr Ec2DirectConnection::getCameraManager()
    {
        return m_cameraManager;
    }

    AbstractLicenseManagerPtr Ec2DirectConnection::getLicenseManager()
    {
        return nullptr;
    }

    AbstractBusinessEventManagerPtr Ec2DirectConnection::getBusinessEventManager()
    {
        return nullptr;
    }

    AbstractUserManagerPtr Ec2DirectConnection::getUserManager()
    {
        return nullptr;
    }

    AbstractLayoutManagerPtr Ec2DirectConnection::getLayoutManager()
    {
        return nullptr;
    }

    AbstractStoredFileManagerPtr Ec2DirectConnection::getStoredFileManager()
    {
        return nullptr;
    }

    ReqID Ec2DirectConnection::setPanicMode( PanicMode value, impl::SimpleHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }

    ReqID Ec2DirectConnection::getCurrentTime( impl::CurrentTimeHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }

    ReqID Ec2DirectConnection::dumpDatabaseAsync( impl::DumpDatabaseHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }

    ReqID Ec2DirectConnection::restoreDatabaseAsync( const QByteArray& dbFile, impl::SimpleHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }

    ReqID Ec2DirectConnection::getSettingsAsync( impl::GetSettingsHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }

    ReqID Ec2DirectConnection::saveSettingsAsync( const QnKvPairList& kvPairs, impl::SimpleHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }
}
