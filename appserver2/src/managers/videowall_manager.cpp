#include "videowall_manager.h"

#include "fixed_url_client_query_processor.h"
#include "server_query_processor.h"

namespace ec2
{

    QnVideowallNotificationManager::QnVideowallNotificationManager() {}

    void QnVideowallNotificationManager::triggerNotification(const QnTransaction<ApiVideowallData>& tran)
    {
        NX_ASSERT(tran.command == ApiCommand::saveVideowall);
        emit addedOrUpdated(tran.params);
    }

    void QnVideowallNotificationManager::triggerNotification(const QnTransaction<ApiIdData>& tran)
    {
        NX_ASSERT(tran.command == ApiCommand::removeVideowall);
        emit removed(tran.params.id);
    }

    void QnVideowallNotificationManager::triggerNotification(const QnTransaction<ApiVideowallControlMessageData>& tran)
    {
        NX_ASSERT(tran.command == ApiCommand::videowallControl);
        emit controlMessage(tran.params);
    }


    template<class QueryProcessorType>
    QnVideowallManager<QueryProcessorType>::QnVideowallManager(QnVideowallNotificationManagerRawPtr &base,
                                                               QueryProcessorType* const queryProcessor,
                                                               const Qn::UserAccessData &userAccessData)
    :
        m_base(base),
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
        m_queryProcessor->getAccess(m_userAccessData).template processQueryAsync<std::nullptr_t, ApiVideowallDataList, decltype(queryDoneHandler)> ( ApiCommand::getVideowalls, nullptr, queryDoneHandler);
        return reqID;
    }

    template<class T>
    int QnVideowallManager<T>::save(const ec2::ApiVideowallData& videowall, impl::SimpleHandlerPtr handler)
    {
        const int reqID = generateRequestID();
        QnTransaction<ApiVideowallData> tran(ApiCommand::saveVideowall, videowall);
        m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(tran, [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
        return reqID;
    }

    template<class T>
    int QnVideowallManager<T>::remove( const QnUuid& id, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        QnTransaction<ApiIdData> tran(ApiCommand::removeVideowall, id);
        m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(tran, [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
        return reqID;
    }

    template<class T>
    int QnVideowallManager<T>::sendControlMessage(const ec2::ApiVideowallControlMessageData& message, impl::SimpleHandlerPtr handler)
    {
        const int reqID = generateRequestID();
        QnTransaction<ApiVideowallControlMessageData> tran(ApiCommand::videowallControl, message);
        m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(tran, [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
        return reqID;
    }

    template class QnVideowallManager<ServerQueryProcessorAccess>;
    template class QnVideowallManager<FixedUrlClientQueryProcessor>;

}
