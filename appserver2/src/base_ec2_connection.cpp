/**********************************************************
* 21 jan 2014
* a.kolesnikov
***********************************************************/

#include "base_ec2_connection.h"

#include "fixed_url_client_query_processor.h"
#include "server_query_processor.h"
#include "common/common_module.h"
#include "transaction/transaction_message_bus.h"


namespace ec2
{
    template<class T>
    BaseEc2Connection<T>::BaseEc2Connection(
        T* queryProcessor,
        const ResourceContext& resCtx )
    :
        m_queryProcessor( queryProcessor ),
        m_resCtx( resCtx ),
        m_licenseManager( new QnLicenseManager<T>(m_queryProcessor) ),
        m_resourceManager( new QnResourceManager<T>(m_queryProcessor, resCtx) ),
        m_mediaServerManager( new QnMediaServerManager<T>(m_queryProcessor, resCtx) ),
        m_cameraManager( new QnCameraManager<T>(m_queryProcessor, resCtx) ),
        m_userManager( new QnUserManager<T>(m_queryProcessor, resCtx) ),
        m_businessEventManager( new QnBusinessEventManager<T>(m_queryProcessor, resCtx) ),
        m_layoutManager( new QnLayoutManager<T>(m_queryProcessor, resCtx) ),
        m_videowallManager( new QnVideowallManager<T>(m_queryProcessor, resCtx) ),
        m_storedFileManager( new QnStoredFileManager<T>(m_queryProcessor, resCtx) ),
        m_updatesManager( new QnUpdatesManager<T>(m_queryProcessor) ),
        m_miscManager( new QnMiscNotificationManager<T>(m_queryProcessor) ),
        m_discoveryManager( new QnDiscoveryNotificationManager<T>(m_queryProcessor) )
    {
        connect (QnTransactionMessageBus::instance(), &QnTransactionMessageBus::peerFound, this, &BaseEc2Connection<T>::remotePeerFound, Qt::DirectConnection);
        connect (QnTransactionMessageBus::instance(), &QnTransactionMessageBus::peerLost,  this, &BaseEc2Connection<T>::remotePeerLost, Qt::DirectConnection);

        m_notificationManager.reset(
            new ECConnectionNotificationManager(
                m_resCtx,
                this, 
                m_licenseManager.get(),
                m_resourceManager.get(),
                m_mediaServerManager.get(),
                m_cameraManager.get(),
                m_userManager.get(),
                m_businessEventManager.get(),
                m_layoutManager.get(),
                m_videowallManager.get(),
                m_storedFileManager.get(),
                m_updatesManager.get() ) );
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
        return m_licenseManager;
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
        return m_layoutManager;
    }

    template<class T>
    AbstractVideowallManagerPtr BaseEc2Connection<T>::getVideowallManager()
    {
        return m_videowallManager;
    }

    template<class T>
    AbstractStoredFileManagerPtr BaseEc2Connection<T>::getStoredFileManager()
    {
        return m_storedFileManager;
    }

    template<class T>
    AbstractUpdatesManagerPtr BaseEc2Connection<T>::getUpdatesManager()
    {
        return m_updatesManager;
    }

    template<class T>
    AbstractMiscManagerPtr BaseEc2Connection<T>::getMiscManager()
    {
        return m_miscManager;
    }

    template<class T>
    AbstractDiscoveryManagerPtr BaseEc2Connection<T>::getDiscoveryManager()
    {
        return m_discoveryManager;
    }

    template<class T>
    int BaseEc2Connection<T>::setPanicMode( Qn::PanicMode value, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        ApiCommand::Value command = ApiCommand::setPanicMode;

        //performing request
        auto tran = prepareTransaction( command, value );

        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( std::mem_fn( &impl::SimpleHandler::done ), handler, reqID, _1) );

        return reqID;
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
    int BaseEc2Connection<T>::getCurrentTime( impl::CurrentTimeHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        auto queryDoneHandler = [reqID, handler]( ErrorCode errorCode, const qint64& currentTime) {
            qint64 outData = 0;
            if( errorCode == ErrorCode::ok )
                outData = currentTime;
            handler->done( reqID, errorCode, outData);
        };
        m_queryProcessor->template processQueryAsync<std::nullptr_t, qint64, decltype(queryDoneHandler)> (
            ApiCommand::getCurrentTime, nullptr, queryDoneHandler );

        return reqID;
    }
    


    template<class T>
    int BaseEc2Connection<T>::dumpDatabaseAsync( impl::DumpDatabaseHandlerPtr /*handler*/ )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }

    template<class T>
    int BaseEc2Connection<T>::restoreDatabaseAsync( const QByteArray& /*dbFile*/, impl::SimpleHandlerPtr /*handler*/ )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }

    template<class T>
    int BaseEc2Connection<T>::getSettingsAsync( impl::GetSettingsHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        auto queryDoneHandler = [reqID, handler]( ErrorCode errorCode, const ApiResourceParamDataList& settings) {
            QnKvPairList outData;
            if( errorCode == ErrorCode::ok )
                fromApiToResourceList(settings, outData);
            handler->done( reqID, errorCode, outData );
        };
        m_queryProcessor->template processQueryAsync<std::nullptr_t, ApiResourceParamDataList, decltype(queryDoneHandler)> ( ApiCommand::getSettings, nullptr, queryDoneHandler);
        return reqID;
    }

    template<class T>
    int BaseEc2Connection<T>::saveSettingsAsync( const QnKvPairList& kvPairs, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        QnTransaction<ApiResourceParamDataList> tran(ApiCommand::saveSettings, true);
        fromResourceListToApi(kvPairs, tran.params);

        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( std::mem_fn( &impl::SimpleHandler::done ), handler, reqID, _1) );

        return reqID;
    }

    template<class T>
    QnTransaction<ApiPanicModeData> BaseEc2Connection<T>::prepareTransaction( ApiCommand::Value command, const Qn::PanicMode& mode)
    {
        QnTransaction<ApiPanicModeData> tran(command, true);
        tran.params.mode = mode;
        return tran;
    }

    template<class T>
    void BaseEc2Connection<T>::addRemotePeer(const QUrl& _url, const QUuid& peerGuid)
    {
        QUrl url(_url);
        url.setPath("/ec2/events");
        QUrlQuery q;
        q.addQueryItem("guid", qnCommon->moduleGUID().toString());
        url.setQuery(q);
        QnTransactionMessageBus::instance()->addConnectionToPeer(url, peerGuid);
    }

    template<class T>
    void BaseEc2Connection<T>::deleteRemotePeer(const QUrl& _url)
    {
        QUrl url(_url);
        url.setPath("/ec2/events");
        QUrlQuery q;
        q.addQueryItem("guid", qnCommon->moduleGUID().toString());
        url.setQuery(q);
        QnTransactionMessageBus::instance()->removeConnectionFromPeer(url);
    }


    template class BaseEc2Connection<FixedUrlClientQueryProcessor>;
    template class BaseEc2Connection<ServerQueryProcessor>;
}
