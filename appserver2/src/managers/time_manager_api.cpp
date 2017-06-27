/**********************************************************
* 27 aug 2014
* a.kolesnikov
***********************************************************/

#include "time_manager_api.h"

#include "ec2_thread_pool.h"
#include "fixed_url_client_query_processor.h"
#include "server_query_processor.h"
#include "time_manager.h"


namespace ec2
{
    template<typename QueryProcessorType>
    QnTimeNotificationManager<QueryProcessorType>::QnTimeNotificationManager(
        TimeSynchronizationManager* timeSyncManager):
        m_timeSyncManager(timeSyncManager)
    {
        connect(timeSyncManager, &TimeSynchronizationManager::primaryTimeServerSelectionRequired,
            this, &QnTimeNotificationManager<QueryProcessorType>::timeServerSelectionRequired,
            Qt::DirectConnection);
        connect(timeSyncManager, &TimeSynchronizationManager::timeChanged,
            this, &QnTimeNotificationManager<QueryProcessorType>::timeChanged,
            Qt::DirectConnection);
        connect(timeSyncManager, &TimeSynchronizationManager::peerTimeChanged,
            this, &QnTimeNotificationManager<QueryProcessorType>::peerTimeChanged,
            Qt::DirectConnection);
    }

    template<typename QueryProcessorType>
    QnTimeNotificationManager<QueryProcessorType>::~QnTimeNotificationManager()
    {
        //safely disconnecting from TimeSynchronizationManager
        m_timeSyncManager->disconnectAndJoin( this );
    }

    template<class QueryProcessorType>
    QnTimeManager<QueryProcessorType>::QnTimeManager(
        QueryProcessorType* queryProcessor,
        TimeSynchronizationManager* timeSyncManager,
        const Qn::UserAccessData &userAccessData)
    :
        m_queryProcessor( queryProcessor ),
        m_timeSyncManager(timeSyncManager),
        m_userAccessData(userAccessData)
    {
    }

    template<class QueryProcessorType>
    QnTimeManager<QueryProcessorType>::~QnTimeManager()
    {
    }

    template<class QueryProcessorType>
    QnPeerTimeInfoList QnTimeManager<QueryProcessorType>::getPeerTimeInfoList() const
    {
        return m_timeSyncManager->getPeerTimeInfoList();
    }

    template<class QueryProcessorType>
    int QnTimeManager<QueryProcessorType>::getCurrentTimeImpl( impl::CurrentTimeHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        nx::utils::concurrent::run(
            Ec2ThreadPool::instance(),
            std::bind( &impl::CurrentTimeHandler::done, handler, reqID, ec2::ErrorCode::ok, m_timeSyncManager->getSyncTime() ) );
        return reqID;
    }

    template<class QueryProcessorType>
    int QnTimeManager<QueryProcessorType>::forcePrimaryTimeServerImpl( const QnUuid& serverGuid, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        using namespace std::placeholders;
        m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
            ApiCommand::forcePrimaryTimeServer,
            ApiIdData(serverGuid),
            std::bind( &impl::SimpleHandler::done, handler, reqID, _1) );

        return reqID;
    }

    template<class QueryProcessorType>
    void QnTimeManager<QueryProcessorType>::forceTimeResync()
    {
        return m_timeSyncManager->forceTimeResync();
    }


    template class QnTimeManager<FixedUrlClientQueryProcessor>;
    template class QnTimeManager<ServerQueryProcessorAccess>;

    template class QnTimeNotificationManager<FixedUrlClientQueryProcessor>;
    template class QnTimeNotificationManager<ServerQueryProcessorAccess>;
}
