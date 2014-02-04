/**********************************************************
* 21 jan 2014
* a.kolesnikov
***********************************************************/

#include "base_ec2_connection.h"

#include "camera_manager.h"
#include "managers/license_manager.h"
#include "media_server_manager.h"
#include "resource_manager.h"
#include "client_query_processor.h"
#include "server_query_processor.h"
#include "user_manager.h"
#include "business_event_manager.h"


namespace ec2
{
    template<class T>
    BaseEc2Connection<T>::BaseEc2Connection(
        T* queryProcessor,
        const ResourceContext& resCtx )
    :
        m_queryProcessor( queryProcessor ),
        m_licenseManager( new QnLicenseManager<T>(m_queryProcessor) ),
        m_resourceManager( new QnResourceManager<T>(m_queryProcessor) ),
        m_mediaServerManager( new QnMediaServerManager<T>(m_queryProcessor, resCtx) ),
        m_cameraManager( new QnCameraManager<T>(m_queryProcessor, resCtx) ),
        m_userManager( new QnUserManager<T>(m_queryProcessor, resCtx) ),
        m_businessEventManager( new QnBusinessEventManager<T>(m_queryProcessor, resCtx) )
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
        return m_businessEventManager;
    }

    template<class T>
    AbstractUserManagerPtr BaseEc2Connection<T>::getUserManager()
    {
        return m_userManager;
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

    /*
    ReqID BaseEc2Connection<ServerQueryProcessor>::getCurrentTime( impl::CurrentTimeHandlerPtr handler )
    {
        const ReqID reqID = generateRequestID();
        qint64 curTime = 0;
        CommonRequestsProcessor::getCurrentTime( nullptr, &curTime );
        QtConcurrent::run( std::bind( std::mem_fn( &impl::CurrentTimeHandler::done ), handler, reqID, ec2::ErrorCode::ok, curTime ) );
        return reqID;
    }
    */
    template <class T>
    ReqID BaseEc2Connection<T>::getCurrentTime( impl::CurrentTimeHandlerPtr handler )
    {
        const ReqID reqID = generateRequestID();

        auto queryDoneHandler = [reqID, handler]( ErrorCode errorCode, const qint64& currentTime) {
            qint64 outData = 0;
            if( errorCode == ErrorCode::ok )
                outData = currentTime;
            handler->done( reqID, errorCode, outData);
        };
        m_queryProcessor->processQueryAsync<nullptr_t, qint64, decltype(queryDoneHandler)> (
            ApiCommand::getCurrentTime, nullptr, queryDoneHandler );

        return reqID;
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
