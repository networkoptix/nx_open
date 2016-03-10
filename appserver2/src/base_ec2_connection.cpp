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
    template<class QueryProcessorType>
    BaseEc2Connection<QueryProcessorType>::BaseEc2Connection(
        QueryProcessorType* queryProcessor,
        const ResourceContext& resCtx )
    :
        m_queryProcessor( queryProcessor ),
        m_resCtx( resCtx ),
        m_licenseManager( new QnLicenseManager<QueryProcessorType>(m_queryProcessor) ),
        m_resourceManager( new QnResourceManager<QueryProcessorType>(m_queryProcessor, resCtx) ),
        m_mediaServerManager( new QnMediaServerManager<QueryProcessorType>(m_queryProcessor, resCtx) ),
        m_cameraManager( new QnCameraManager<QueryProcessorType>(m_queryProcessor) ),
        m_userManager( new QnUserManager<QueryProcessorType>(m_queryProcessor) ),
        m_businessEventManager( new QnBusinessEventManager<QueryProcessorType>(m_queryProcessor) ),
        m_layoutManager( new QnLayoutManager<QueryProcessorType>(m_queryProcessor) ),
        m_videowallManager( new QnVideowallManager<QueryProcessorType>(m_queryProcessor) ),
        m_webPageManager ( new QnWebPageManager<QueryProcessorType>(m_queryProcessor) ),
        m_storedFileManager( new QnStoredFileManager<QueryProcessorType>(m_queryProcessor) ),
        m_updatesManager( new QnUpdatesManager<QueryProcessorType>(m_queryProcessor) ),
        m_miscManager( new QnMiscManager<QueryProcessorType>(m_queryProcessor) ),
        m_discoveryManager( new QnDiscoveryManager<QueryProcessorType>(m_queryProcessor) ),
        m_timeManager( new QnTimeManager<QueryProcessorType>(m_queryProcessor) )
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
                m_webPageManager.get(),
                m_storedFileManager.get(),
                m_updatesManager.get(),
                m_miscManager.get(),
                m_discoveryManager.get() ) );

        m_auditManager.reset(new ECConnectionAuditManager(this));
    }

    template<class QueryProcessorType>
    BaseEc2Connection<QueryProcessorType>::~BaseEc2Connection()
    {}

    template<class QueryProcessorType>
    void BaseEc2Connection<QueryProcessorType>::startReceivingNotifications() {
        connect(QnTransactionMessageBus::instance(),    &QnTransactionMessageBus::peerFound,                this,   &BaseEc2Connection<QueryProcessorType>::remotePeerFound,         Qt::DirectConnection);
        connect(QnTransactionMessageBus::instance(),    &QnTransactionMessageBus::peerLost,                 this,   &BaseEc2Connection<QueryProcessorType>::remotePeerLost,          Qt::DirectConnection);
        connect(QnTransactionMessageBus::instance(),    &QnTransactionMessageBus::remotePeerUnauthorized,   this,   &BaseEc2Connection<QueryProcessorType>::remotePeerUnauthorized,  Qt::DirectConnection);
        QnTransactionMessageBus::instance()->start();
    }

    template<class QueryProcessorType>
    void BaseEc2Connection<QueryProcessorType>::stopReceivingNotifications() {
        QnTransactionMessageBus::instance()->disconnectAndJoin( this );
        QnTransactionMessageBus::instance()->stop();
    }

    template<class QueryProcessorType>
    AbstractResourceManagerPtr BaseEc2Connection<QueryProcessorType>::getResourceManager()
    {
        return m_resourceManager;
    }

    template<class QueryProcessorType>
    AbstractMediaServerManagerPtr BaseEc2Connection<QueryProcessorType>::getMediaServerManager()
    {
        return m_mediaServerManager;
    }

    template<class QueryProcessorType>
    AbstractCameraManagerPtr BaseEc2Connection<QueryProcessorType>::getCameraManager()
    {
        return m_cameraManager;
    }

    template<class QueryProcessorType>
    AbstractLicenseManagerPtr BaseEc2Connection<QueryProcessorType>::getLicenseManager()
    {
        return m_licenseManager;
    }

    template<class QueryProcessorType>
    AbstractBusinessEventManagerPtr BaseEc2Connection<QueryProcessorType>::getBusinessEventManager()
    {
        return m_businessEventManager;
    }

    template<class QueryProcessorType>
    AbstractUserManagerPtr BaseEc2Connection<QueryProcessorType>::getUserManager()
    {
        return m_userManager;
    }

    template<class QueryProcessorType>
    AbstractLayoutManagerPtr BaseEc2Connection<QueryProcessorType>::getLayoutManager()
    {
        return m_layoutManager;
    }

    template<class QueryProcessorType>
    AbstractVideowallManagerPtr BaseEc2Connection<QueryProcessorType>::getVideowallManager()
    {
        return m_videowallManager;
    }

    template<class QueryProcessorType>
    AbstractWebPageManagerPtr BaseEc2Connection<QueryProcessorType>::getWebPageManager()
    {
        return m_webPageManager;
    }

    template<class QueryProcessorType>
    AbstractStoredFileManagerPtr BaseEc2Connection<QueryProcessorType>::getStoredFileManager()
    {
        return m_storedFileManager;
    }

    template<class QueryProcessorType>
    AbstractUpdatesManagerPtr BaseEc2Connection<QueryProcessorType>::getUpdatesManager()
    {
        return m_updatesManager;
    }

    template<class QueryProcessorType>
    AbstractMiscManagerPtr BaseEc2Connection<QueryProcessorType>::getMiscManager()
    {
        return m_miscManager;
    }

    template<class QueryProcessorType>
    AbstractDiscoveryManagerPtr BaseEc2Connection<QueryProcessorType>::getDiscoveryManager()
    {
        return m_discoveryManager;
    }

    template<class QueryProcessorType>
    AbstractTimeManagerPtr BaseEc2Connection<QueryProcessorType>::getTimeManager()
    {
        return m_timeManager;
    }

    template<class QueryProcessorType>
    int BaseEc2Connection<QueryProcessorType>::dumpDatabaseAsync( impl::DumpDatabaseHandlerPtr handler )
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

    template<class QueryProcessorType>
    int BaseEc2Connection<QueryProcessorType>::dumpDatabaseToFileAsync( const QString& dumpFilePath, impl::SimpleHandlerPtr handler )
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

    template<class QueryProcessorType>
    int BaseEc2Connection<QueryProcessorType>::restoreDatabaseAsync( const ec2::ApiDatabaseDumpData& data, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        QnTransaction<ApiDatabaseDumpData> tran(ApiCommand::restoreDatabase);
        tran.isLocal = true;
        tran.params = data;

        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( std::mem_fn( &impl::SimpleHandler::done ), handler, reqID, _1) );

        return reqID;
    }

    template<class QueryProcessorType>
    void BaseEc2Connection<QueryProcessorType>::addRemotePeer(const QUrl& _url)
    {
        QUrl url(_url);
        url.setPath("/ec2/events");
        QUrlQuery q;
        url.setQuery(q);
        QnTransactionMessageBus::instance()->addConnectionToPeer(url);
    }

    template<class QueryProcessorType>
    void BaseEc2Connection<QueryProcessorType>::deleteRemotePeer(const QUrl& _url)
    {
        QUrl url(_url);
        url.setPath("/ec2/events");
        QUrlQuery q;
        url.setQuery(q);
        QnTransactionMessageBus::instance()->removeConnectionFromPeer(url);
    }


    template<class QueryProcessorType>
    void ec2::BaseEc2Connection<QueryProcessorType>::sendRuntimeData(const ec2::ApiRuntimeData &data)
    {
        ec2::QnTransaction<ec2::ApiRuntimeData> tran(ec2::ApiCommand::runtimeInfoChanged);
        tran.params = data;
        qnTransactionBus->sendTransaction(tran);
    }

    template<class QueryProcessorType>
    qint64 ec2::BaseEc2Connection<QueryProcessorType>::getTransactionLogTime() const {
        return transactionLog ? transactionLog->getTransactionLogTime() : -1;
    }

    template<class QueryProcessorType>
    void ec2::BaseEc2Connection<QueryProcessorType>::setTransactionLogTime(qint64 value)
    {
        if (transactionLog)
            transactionLog->setTransactionLogTime(value);
    }

    template<class QueryProcessorType>
    QnUuid ec2::BaseEc2Connection<QueryProcessorType>::routeToPeerVia(const QnUuid& dstPeer, int* distance) const
    {
        return qnTransactionBus ? qnTransactionBus->routeToPeerVia(dstPeer, distance) : QnUuid();
    }



    template class BaseEc2Connection<FixedUrlClientQueryProcessor>;
    template class BaseEc2Connection<ServerQueryProcessor>;
}
