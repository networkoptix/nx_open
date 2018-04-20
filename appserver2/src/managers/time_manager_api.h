#pragma once

#include <nx_ec/ec_api.h>
#include <nx/utils/concurrent.h>

namespace ec2
{
    class TimeSynchronizationManager;

    template<class QueryProcessorType>
    class QnTimeManager
    :
        public AbstractTimeManager
    {
    public:
        QnTimeManager(
            QueryProcessorType* queryProcessor,
            TimeSynchronizationManager* timeSyncManager,
            const Qn::UserAccessData &userAccessData );
        virtual ~QnTimeManager();

        //!Implementation of AbstractTimeManager::getCurrentTimeImpl
        virtual int getCurrentTimeImpl( impl::CurrentTimeHandlerPtr handler ) override;
        //!Implementation of AbstractTimeManager::forcePrimaryTimeServerImpl
        virtual int forcePrimaryTimeServerImpl( const QnUuid& serverGuid, impl::SimpleHandlerPtr handler ) override;
        //!Implementation of AbstractTimeManager::forceTimeResync
        virtual void forceTimeResync() override;

    private:
        QueryProcessorType* m_queryProcessor;
        TimeSynchronizationManager* m_timeSyncManager;
        Qn::UserAccessData m_userAccessData;
    };

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
            nx::vms::api::IdData(serverGuid),
            std::bind( &impl::SimpleHandler::done, handler, reqID, _1) );

        return reqID;
    }

    template<class QueryProcessorType>
    void QnTimeManager<QueryProcessorType>::forceTimeResync()
    {
        return m_timeSyncManager->forceTimeResync();
    }

} // namespace ec2

