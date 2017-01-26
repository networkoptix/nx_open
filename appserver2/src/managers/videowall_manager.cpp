#include "videowall_manager.h"

#include "fixed_url_client_query_processor.h"
#include "server_query_processor.h"

namespace ec2
{

    QnVideowallNotificationManager::QnVideowallNotificationManager() {}

    void QnVideowallNotificationManager::triggerNotification(const QnTransaction<ApiVideowallData>& tran, NotificationSource source)
    {
        NX_ASSERT(tran.command == ApiCommand::saveVideowall);
        emit addedOrUpdated(tran.params, source);
    }

    void QnVideowallNotificationManager::triggerNotification(const QnTransaction<ApiIdData>& tran, NotificationSource /*source*/)
    {
        NX_ASSERT(tran.command == ApiCommand::removeVideowall);
        emit removed(tran.params.id);
    }

    void QnVideowallNotificationManager::triggerNotification(const QnTransaction<ApiVideowallControlMessageData>& tran, NotificationSource /*source*/)
    {
        NX_ASSERT(tran.command == ApiCommand::videowallControl);
        emit controlMessage(tran.params);
    }


    template<class QueryProcessorType>
    QnVideowallManager<QueryProcessorType>::QnVideowallManager(QueryProcessorType* const queryProcessor,
                                                               const Qn::UserAccessData &userAccessData)
    :
        m_queryProcessor(queryProcessor),
        m_userAccessData(userAccessData)
    {}

    template<class QueryProcessorType>
    int QnVideowallManager<QueryProcessorType>::getVideowalls( impl::GetVideowallsHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        auto queryDoneHandler = [reqID, handler](ErrorCode errorCode, const ApiVideowallDataList& videowalls)
        {
            handler->done(reqID, errorCode, videowalls);
        };
        m_queryProcessor->getAccess(m_userAccessData).template processQueryAsync<QnUuid, ApiVideowallDataList, decltype(queryDoneHandler)> ( ApiCommand::getVideowalls, QnUuid(), queryDoneHandler);
        return reqID;
    }

    template<class T>
    int QnVideowallManager<T>::save(const ec2::ApiVideowallData& videowall, impl::SimpleHandlerPtr handler)
    {
        const int reqID = generateRequestID();
        m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
            ApiCommand::saveVideowall,
            videowall,
            [handler, reqID](ec2::ErrorCode errorCode)
            {
                handler->done(reqID, errorCode);
            });
        return reqID;
    }

    template<class T>
    int QnVideowallManager<T>::remove( const QnUuid& id, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
            ApiCommand::removeVideowall, ApiIdData(id),
            [handler, reqID](ec2::ErrorCode errorCode)
            {
                handler->done(reqID, errorCode);
            });
        return reqID;
    }

    template<class T>
    int QnVideowallManager<T>::sendControlMessage(const ec2::ApiVideowallControlMessageData& message, impl::SimpleHandlerPtr handler)
    {
        const int reqID = generateRequestID();
        m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
            ApiCommand::videowallControl, message,
            [handler, reqID](ec2::ErrorCode errorCode)
            {
                handler->done(reqID, errorCode);
            });
        return reqID;
    }

    template class QnVideowallManager<ServerQueryProcessorAccess>;
    template class QnVideowallManager<FixedUrlClientQueryProcessor>;

}
