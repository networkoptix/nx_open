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
    template<class QueryProcessorType>
    QnTimeManager<QueryProcessorType>::QnTimeManager( QueryProcessorType* queryProcessor )
    :
        m_queryProcessor( queryProcessor )
    {
        connect (TimeSynchronizationManager::instance(), &TimeSynchronizationManager::primaryTimeServerSelectionRequired,
                 this, &QnTimeManager<QueryProcessorType>::timeServerSelectionRequired,
                 Qt::DirectConnection );
        connect (TimeSynchronizationManager::instance(), &TimeSynchronizationManager::timeChanged,
                 this, &QnTimeManager<QueryProcessorType>::timeChanged,
                 Qt::DirectConnection );
        connect (TimeSynchronizationManager::instance(), &TimeSynchronizationManager::peerTimeChanged,
                 this, &QnTimeManager<QueryProcessorType>::peerTimeChanged,
                 Qt::DirectConnection );
    }

    template<class QueryProcessorType>
    QnTimeManager<QueryProcessorType>::~QnTimeManager()
    {
        //safely disconnecting from TimeSynchronizationManager
        TimeSynchronizationManager::instance()->disconnectAndJoin( this );
    }

    template<class QueryProcessorType>
    QnPeerTimeInfoList QnTimeManager<QueryProcessorType>::getPeerTimeInfoList() const
    {
        return TimeSynchronizationManager::instance()->getPeerTimeInfoList();
    }
    
    template<class QueryProcessorType>
    int QnTimeManager<QueryProcessorType>::getCurrentTimeImpl( impl::CurrentTimeHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        QnConcurrent::run(
            Ec2ThreadPool::instance(),
            std::bind( &impl::CurrentTimeHandler::done, handler, reqID, ec2::ErrorCode::ok, TimeSynchronizationManager::instance()->getSyncTime() ) );
        return reqID;
    }
    
    template<class QueryProcessorType>
    int QnTimeManager<QueryProcessorType>::forcePrimaryTimeServerImpl( const QnUuid& serverGuid, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        QnTransaction<ApiIdData> tran( ApiCommand::forcePrimaryTimeServer );
        tran.params.id = serverGuid;

        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( &impl::SimpleHandler::done, handler, reqID, _1) );

        return reqID;
    }


    template class QnTimeManager<FixedUrlClientQueryProcessor>;
    template class QnTimeManager<ServerQueryProcessor>;
}
