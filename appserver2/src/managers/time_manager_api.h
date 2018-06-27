#pragma once

#include <nx_ec/ec_api.h>
#include <nx/utils/concurrent.h>
#include <nx/time_sync/time_sync_manager.h>
#include <transaction/transaction.h>
#include <ec2_thread_pool.h>

namespace ec2
{
    template<class QueryProcessorType>
    class QnTimeManager
    :
        public AbstractTimeManager
    {
    public:
        QnTimeManager(
            QueryProcessorType* queryProcessor,
            nx::time_sync::TimeSyncManager* timeSyncManager,
            const Qn::UserAccessData &userAccessData );
        virtual ~QnTimeManager();

        //!Implementation of AbstractTimeManager::getCurrentTimeImpl
        virtual int getCurrentTimeImpl( impl::CurrentTimeHandlerPtr handler ) override;
        //!Implementation of AbstractTimeManager::forcePrimaryTimeServerImpl
        virtual int forcePrimaryTimeServerImpl( const QnUuid& serverGuid, impl::SimpleHandlerPtr handler ) override;

    private:
        QueryProcessorType* m_queryProcessor;
        nx::time_sync::TimeSyncManager* m_timeSyncManager;
        Qn::UserAccessData m_userAccessData;
    };

    template<class QueryProcessorType>
    QnTimeManager<QueryProcessorType>::QnTimeManager(
        QueryProcessorType* queryProcessor,
        nx::time_sync::TimeSyncManager* timeSyncManager,
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
    int QnTimeManager<QueryProcessorType>::getCurrentTimeImpl(impl::CurrentTimeHandlerPtr handler)
    {
        const int reqId = generateRequestID();
        nx::utils::concurrent::run(
            Ec2ThreadPool::instance(),
            [this, handler, reqId]()
            {
                handler->done(reqId, ec2::ErrorCode::ok, m_timeSyncManager->getSyncTime().count());
            });
        return reqId;
    }

} // namespace ec2

