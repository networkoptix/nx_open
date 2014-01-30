/**********************************************************
* 21 jan 2014
* a.kolesnikov
***********************************************************/

#include "base_ec2_connection.h"

#include "client_query_processor.h"
#include "server_query_processor.h"


namespace ec2
{
    template<class T>
    BaseEc2Connection<T>::BaseEc2Connection( T* queryProcessor, const QnResourceFactoryPtr& resourceFactory )
    :
        m_queryProcessor( queryProcessor ),
        m_resourceManager( new QnResourceManager<T>(m_queryProcessor) ),
        m_mediaServerManager( new QnMediaServerManager<T>(m_queryProcessor) ),
        m_cameraManager( new QnCameraManager<T>(m_queryProcessor, resourceFactory) )
    {
    }

    template<class T>
    AbstractResourceManagerPtr BaseEc2Connection<T>::getResourceManager()
    {
        return m_resourceManager;
    }

    template<class T>
    AbstractMediaServerManagerPtr BaseEc2Connection<T>::getMediaServerManager()
    {
        return m_mediaServerManager;
    }

    template<class T>
    AbstractCameraManagerPtr BaseEc2Connection<T>::getCameraManager()
    {
        return m_cameraManager;
    }

    template<class T>
    AbstractLicenseManagerPtr BaseEc2Connection<T>::getLicenseManager()
    {
        return nullptr;
    }

    template<class T>
    AbstractBusinessEventManagerPtr BaseEc2Connection<T>::getBusinessEventManager()
    {
        return nullptr;
    }

    template<class T>
    AbstractUserManagerPtr BaseEc2Connection<T>::getUserManager()
    {
        return nullptr;
    }

    template<class T>
    AbstractLayoutManagerPtr BaseEc2Connection<T>::getLayoutManager()
    {
        return nullptr;
    }

    template<class T>
    AbstractStoredFileManagerPtr BaseEc2Connection<T>::getStoredFileManager()
    {
        return nullptr;
    }

    template<class T>
    ReqID BaseEc2Connection<T>::setPanicMode( PanicMode value, impl::SimpleHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }

    template<class T>
    ReqID BaseEc2Connection<T>::getCurrentTime( impl::CurrentTimeHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }

    template<class T>
    ReqID BaseEc2Connection<T>::dumpDatabaseAsync( impl::DumpDatabaseHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }

    template<class T>
    ReqID BaseEc2Connection<T>::restoreDatabaseAsync( const QByteArray& dbFile, impl::SimpleHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }

    template<class T>
    ReqID BaseEc2Connection<T>::getSettingsAsync( impl::GetSettingsHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }

    template<class T>
    ReqID BaseEc2Connection<T>::saveSettingsAsync( const QnKvPairList& kvPairs, impl::SimpleHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }


    template class BaseEc2Connection<ClientQueryProcessor>;
    template class BaseEc2Connection<ServerQueryProcessor>;
}
