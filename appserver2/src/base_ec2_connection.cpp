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
#include "managers/time_manager_api.h"
#include "nx_ec/data/api_data.h"


namespace ec2
{
    template<class QueryProcessorType>
    BaseEc2Connection<QueryProcessorType>::BaseEc2Connection(
        QueryProcessorType* queryProcessor)
    :
        m_queryProcessor( queryProcessor ),
        m_licenseNotificationManager(new QnLicenseNotificationManager),
        m_resourceNotificationManager(new QnResourceNotificationManager),
        m_mediaServerNotificationManager(new QnMediaServerNotificationManager),
        m_cameraNotificationManager(new QnCameraNotificationManager),
        m_userNotificationManager(new QnUserNotificationManager),
        m_businessEventNotificationManager(new QnBusinessEventNotificationManager),
        m_layoutNotificationManager(new QnLayoutNotificationManager),
        m_videowallNotificationManager(new QnVideowallNotificationManager),
        m_webPageNotificationManager(new QnWebPageNotificationManager),
        m_storedFileNotificationManager(new QnStoredFileNotificationManager),
        m_updatesNotificationManager(new QnUpdatesNotificationManager),
        m_miscNotificationManager(new QnMiscNotificationManager),
        m_discoveryNotificationManager(new QnDiscoveryNotificationManager),
        m_timeNotificationManager(new QnTimeNotificationManager<QueryProcessorType>)
    {
        m_notificationManager.reset(
            new ECConnectionNotificationManager(
                this,
                m_licenseNotificationManager.get(),
                m_resourceNotificationManager.get(),
                m_mediaServerNotificationManager.get(),
                m_cameraNotificationManager.get(),
                m_userNotificationManager.get(),
                m_businessEventNotificationManager.get(),
                m_layoutNotificationManager.get(),
                m_videowallNotificationManager.get(),
                m_webPageNotificationManager.get(),
                m_storedFileNotificationManager.get(),
                m_updatesNotificationManager.get(),
                m_miscNotificationManager.get(),
                m_discoveryNotificationManager.get() ) );

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
    AbstractResourceManagerPtr BaseEc2Connection<QueryProcessorType>::getResourceManager(const Qn::UserAccessData &userAccessData)
    {
        return std::make_shared<QnResourceManager<QueryProcessorType>>(m_queryProcessor, userAccessData);
    }

    template<class QueryProcessorType>
    AbstractResourceNotificationManagerPtr BaseEc2Connection<QueryProcessorType>::getResourceNotificationManager()
    {
        return m_resourceNotificationManager;
    }

    template<class QueryProcessorType>
    AbstractMediaServerManagerPtr BaseEc2Connection<QueryProcessorType>::getMediaServerManager(const Qn::UserAccessData &userAccessData)
    {
        return std::make_shared<QnMediaServerManager<QueryProcessorType>>(m_queryProcessor, userAccessData);
    }

    template<class QueryProcessorType>
    AbstractMediaServerNotificationManagerPtr BaseEc2Connection<QueryProcessorType>::getMediaServerNotificationManager()
    {
        return m_mediaServerNotificationManager;
    }

    template<class QueryProcessorType>
    AbstractCameraManagerPtr BaseEc2Connection<QueryProcessorType>::getCameraManager(const Qn::UserAccessData &userAccessData)
    {
        return std::make_shared<QnCameraManager<QueryProcessorType>>(m_queryProcessor, userAccessData);
    }

    template<class QueryProcessorType>
    AbstractCameraNotificationManagerPtr BaseEc2Connection<QueryProcessorType>::getCameraNotificationManager()
    {
        return m_cameraNotificationManager;
    }

    template<class QueryProcessorType>
    AbstractLicenseManagerPtr BaseEc2Connection<QueryProcessorType>::getLicenseManager(const Qn::UserAccessData &userAccessData)
    {
        return std::make_shared<QnLicenseManager<QueryProcessorType>>(m_queryProcessor, userAccessData);
    }

    template<class QueryProcessorType>
    AbstractLicenseNotificationManagerPtr BaseEc2Connection<QueryProcessorType>::getLicenseNotificationManager()
    {
        return m_licenseNotificationManager;
    }

    template<class QueryProcessorType>
    AbstractBusinessEventManagerPtr BaseEc2Connection<QueryProcessorType>::getBusinessEventManager(const Qn::UserAccessData &userAccessData)
    {
        return std::make_shared<QnBusinessEventManager<QueryProcessorType>>(m_queryProcessor, userAccessData);
    }

    template<class QueryProcessorType>
    AbstractBusinessEventNotificationManagerPtr BaseEc2Connection<QueryProcessorType>::getBusinessEventNotificationManager()
    {
        return m_businessEventNotificationManager;
    }

    template<class QueryProcessorType>
    AbstractUserManagerPtr BaseEc2Connection<QueryProcessorType>::getUserManager(const Qn::UserAccessData &userAccessData)
    {
        return std::make_shared<QnUserManager<QueryProcessorType>>(m_queryProcessor, userAccessData);
    }

    template<class QueryProcessorType>
    AbstractUserNotificationManagerPtr BaseEc2Connection<QueryProcessorType>::getUserNotificationManager()
    {
        return m_userNotificationManager;
    }

    template<class QueryProcessorType>
    AbstractLayoutManagerPtr BaseEc2Connection<QueryProcessorType>::getLayoutManager(const Qn::UserAccessData &userAccessData)
    {
        return std::make_shared<QnLayoutManager<QueryProcessorType>>(m_queryProcessor, userAccessData);
    }

    template<class QueryProcessorType>
    AbstractLayoutNotificationManagerPtr BaseEc2Connection<QueryProcessorType>::getLayoutNotificationManager()
    {
        return m_layoutNotificationManager;
    }

    template<class QueryProcessorType>
    AbstractVideowallManagerPtr BaseEc2Connection<QueryProcessorType>::getVideowallManager(const Qn::UserAccessData &userAccessData)
    {
        return std::make_shared<QnVideowallManager<QueryProcessorType>>(m_queryProcessor, userAccessData);
    }

    template<typename QueryProcessorType>
    AbstractVideowallNotificationManagerPtr BaseEc2Connection<QueryProcessorType>::getVideowallNotificationManager()
    {
        return m_videowallNotificationManager;
    }

    template<class QueryProcessorType>
    AbstractWebPageManagerPtr BaseEc2Connection<QueryProcessorType>::getWebPageManager(const Qn::UserAccessData &userAccessData)
    {
        return std::make_shared<QnWebPageManager<QueryProcessorType>>(m_queryProcessor, userAccessData);
    }

    template<class QueryProcessorType>
    AbstractStoredFileManagerPtr BaseEc2Connection<QueryProcessorType>::getStoredFileManager(const Qn::UserAccessData &userAccessData)
    {
        return std::make_shared<QnStoredFileManager<QueryProcessorType>>(m_queryProcessor, userAccessData);
    }

    template<typename QueryProcessorType>
    AbstractStoredFileNotificationManagerPtr BaseEc2Connection<QueryProcessorType>::getStoredFileNotificationManager()
    {
        return m_storedFileNotificationManager;
    }

    template<class QueryProcessorType>
    AbstractUpdatesNotificationManagerPtr BaseEc2Connection<QueryProcessorType>::getUpdatesNotificationManager()
    {
        return m_updatesNotificationManager;
    }

    template<class QueryProcessorType>
    AbstractUpdatesManagerPtr BaseEc2Connection<QueryProcessorType>::getUpdatesManager(const Qn::UserAccessData &userAccessData)
    {
        return std::make_shared<QnUpdatesManager<QueryProcessorType>>(m_queryProcessor, userAccessData);
    }

    template<class QueryProcessorType>
    AbstractMiscManagerPtr BaseEc2Connection<QueryProcessorType>::getMiscManager(const Qn::UserAccessData &userAccessData)
    {
        return std::make_shared<QnMiscManager<QueryProcessorType>>(m_queryProcessor, userAccessData);
    }

    template<class QueryProcessorType>
    AbstractMiscNotificationManagerPtr BaseEc2Connection<QueryProcessorType>::getMiscNotificationManager()
    {
        return m_miscNotificationManager;
    }

    template<class QueryProcessorType>
    AbstractDiscoveryManagerPtr BaseEc2Connection<QueryProcessorType>::getDiscoveryManager(const Qn::UserAccessData &userAccessData)
    {
        return std::make_shared<QnDiscoveryManager<QueryProcessorType>>(m_queryProcessor, userAccessData);
    }

    template<class QueryProcessorType>
    AbstractDiscoveryNotificationManagerPtr BaseEc2Connection<QueryProcessorType>::getDiscoveryNotificationManager()
    {
        return m_discoveryNotificationManager;
    }

    template<class QueryProcessorType>
    AbstractTimeManagerPtr BaseEc2Connection<QueryProcessorType>::getTimeManager(const Qn::UserAccessData &userAccessData)
    {
        return std::make_shared<QnTimeManager<QueryProcessorType>>(m_queryProcessor, userAccessData);
    }

    template<class QueryProcessorType>
    AbstractTimeNotificationManagerPtr BaseEc2Connection<QueryProcessorType>::getTimeNotificationManager()
    {
        return m_timeNotificationManager;
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
        m_queryProcessor->getAccess(Qn::kSuperUserAccess).template processQueryAsync<std::nullptr_t, ApiDatabaseDumpData, decltype(queryDoneHandler)> (
            ApiCommand::dumpDatabase, nullptr, queryDoneHandler);
        return reqID;
    }

    template<class QueryProcessorType>
    int BaseEc2Connection<QueryProcessorType>::dumpDatabaseToFileAsync( const QString& dumpFilePath, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        ApiStoredFilePath dumpFilePathData;
        dumpFilePathData.path = dumpFilePath;

        auto queryDoneHandler = [reqID, handler]( ErrorCode errorCode, const ApiDatabaseDumpToFileData&/*dumpFileSize*/ ) {
            handler->done( reqID, errorCode );
        };
        m_queryProcessor->getAccess(Qn::kSuperUserAccess).template processQueryAsync<ApiStoredFilePath, ApiDatabaseDumpToFileData, decltype(queryDoneHandler)> (
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
        m_queryProcessor->getAccess(Qn::kSuperUserAccess).processUpdateAsync( tran, std::bind( std::mem_fn( &impl::SimpleHandler::done ), handler, reqID, _1) );

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
    template class BaseEc2Connection<ServerQueryProcessorAccess>;
}
