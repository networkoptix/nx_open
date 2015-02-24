/**********************************************************
* 21 jan 2014
* a.kolesnikov
***********************************************************/

#include "base_ec2_connection.h"

#include <utils/common/concurrent.h>

#include "ec2_thread_pool.h"
#include "fixed_url_client_query_processor.h"
#include "server_query_processor.h"
#include "common/common_module.h"
#include "transaction/transaction_message_bus.h"
#include "managers/time_manager.h"
#include "nx_ec/data/api_data.h"


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
        m_miscManager( new QnMiscManager<T>(m_queryProcessor) ),
        m_discoveryManager( new QnDiscoveryManager<T>(m_queryProcessor) ),
        m_timeManager( new QnTimeManager<T>(m_queryProcessor) )
    {
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
                m_updatesManager.get(),
                m_miscManager.get(),
                m_discoveryManager.get() ) );
    }

    template<class T>
    BaseEc2Connection<T>::~BaseEc2Connection()
    {}

    template<class T>
    void BaseEc2Connection<T>::startReceivingNotifications() {
        connect(QnTransactionMessageBus::instance(),    &QnTransactionMessageBus::peerFound,                this,   &BaseEc2Connection<T>::remotePeerFound,         Qt::DirectConnection);
        connect(QnTransactionMessageBus::instance(),    &QnTransactionMessageBus::peerLost,                 this,   &BaseEc2Connection<T>::remotePeerLost,          Qt::DirectConnection);
        connect(QnTransactionMessageBus::instance(),    &QnTransactionMessageBus::remotePeerUnauthorized,   this,   &BaseEc2Connection<T>::remotePeerUnauthorized,  Qt::DirectConnection);
        QnTransactionMessageBus::instance()->start();
    }

    template<class T>
    void BaseEc2Connection<T>::stopReceivingNotifications() {
        QnTransactionMessageBus::instance()->disconnectAndJoin( this );
        QnTransactionMessageBus::instance()->stop();
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
    AbstractTimeManagerPtr BaseEc2Connection<T>::getTimeManager()
    {
        return m_timeManager;
    }

    template<class T>
    int BaseEc2Connection<T>::dumpDatabaseAsync( impl::DumpDatabaseHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        auto queryDoneHandler = [reqID, handler]( ErrorCode errorCode, const ApiDatabaseDumpData& data) {
            ApiDatabaseDumpData outData;
            if( errorCode == ErrorCode::ok )
                outData = data;
            handler->done( reqID, errorCode, outData );
        };
        m_queryProcessor->template processQueryAsync<std::nullptr_t, ApiDatabaseDumpData, decltype(queryDoneHandler)> ( 
            ApiCommand::dumpDatabase, nullptr, queryDoneHandler);
        return reqID;
    }

    template<class T>
    int BaseEc2Connection<T>::dumpDatabaseToFileAsync( const QString& dumpFilePath, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        ApiStoredFilePath dumpFilePathData;
        dumpFilePathData.path = dumpFilePath;

        auto queryDoneHandler = [reqID, handler]( ErrorCode errorCode, qint64 /*dumpFileSize*/ ) {
            handler->done( reqID, errorCode );
        };
        m_queryProcessor->template processQueryAsync<ApiStoredFilePath, qint64, decltype(queryDoneHandler)> ( 
            ApiCommand::dumpDatabaseToFile, dumpFilePathData, queryDoneHandler );

        return reqID;
    }

    template<class T>
    int BaseEc2Connection<T>::restoreDatabaseAsync( const ec2::ApiDatabaseDumpData& data, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        QnTransaction<ApiDatabaseDumpData> tran(ApiCommand::restoreDatabase);
        tran.isLocal = true;
        tran.params = data;

        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( std::mem_fn( &impl::SimpleHandler::done ), handler, reqID, _1) );

        return reqID;
    }

    template<class T>
    void BaseEc2Connection<T>::addRemotePeer(const QUrl& _url)
    {
        QUrl url(_url);
        url.setPath("/ec2/events");
        QUrlQuery q;
        url.setQuery(q);
        QnTransactionMessageBus::instance()->addConnectionToPeer(url);
    }

    template<class T>
    void BaseEc2Connection<T>::deleteRemotePeer(const QUrl& _url)
    {
        QUrl url(_url);
        url.setPath("/ec2/events");
        QUrlQuery q;
        url.setQuery(q);
        QnTransactionMessageBus::instance()->removeConnectionFromPeer(url);
    }


    template<class T>
    void ec2::BaseEc2Connection<T>::sendRuntimeData(const ec2::ApiRuntimeData &data)
    {
        ec2::QnTransaction<ec2::ApiRuntimeData> tran(ec2::ApiCommand::runtimeInfoChanged);
        tran.params = data;
        qnTransactionBus->sendTransaction(tran);
    }

    template<class T>
    qint64 ec2::BaseEc2Connection<T>::getTransactionLogTime() const {
        return transactionLog ? transactionLog->getTransactionLogTime() : -1;
    }

    template<class T>
    void ec2::BaseEc2Connection<T>::setTransactionLogTime(qint64 value)
    {
        if (transactionLog)
            transactionLog->setTransactionLogTime(value);
    }


    template class BaseEc2Connection<FixedUrlClientQueryProcessor>;
    template class BaseEc2Connection<ServerQueryProcessor>;
}
