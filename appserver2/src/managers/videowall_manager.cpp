#include "videowall_manager.h"

#include "fixed_url_client_query_processor.h"
#include "server_query_processor.h"

namespace ec2
{

    QnVideowallNotificationManager::QnVideowallNotificationManager() {}

    void QnVideowallNotificationManager::triggerNotification(const QnTransaction<ApiVideowallData>& tran)
    {
        assert(tran.command == ApiCommand::saveVideowall);
        emit addedOrUpdated(tran.params);
    }

    void QnVideowallNotificationManager::triggerNotification(const QnTransaction<ApiIdData>& tran)
    {
        assert(tran.command == ApiCommand::removeVideowall);
        emit removed(tran.params.id);
    }

    void QnVideowallNotificationManager::triggerNotification(const QnTransaction<ApiVideowallControlMessageData>& tran)
    {
        assert(tran.command == ApiCommand::videowallControl);
        emit controlMessage(tran.params);
    }


    template<class QueryProcessorType>
    QnVideowallManager<QueryProcessorType>::QnVideowallManager(QueryProcessorType* const queryProcessor)
    :
        QnVideowallNotificationManager(),
        m_queryProcessor( queryProcessor )
    {}

    template<class QueryProcessorType>
    int QnVideowallManager<QueryProcessorType>::getVideowalls( impl::GetVideowallsHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        auto queryDoneHandler = [reqID, handler](ErrorCode errorCode, const ApiVideowallDataList& videowalls)
        {
            handler->done(reqID, errorCode, videowalls);
        };
        m_queryProcessor->template processQueryAsync<std::nullptr_t, ApiVideowallDataList, decltype(queryDoneHandler)> ( ApiCommand::getVideowalls, nullptr, queryDoneHandler);
        return reqID;
    }

    template<class T>
    int QnVideowallManager<T>::save(const ec2::ApiVideowallData& videowall, impl::SimpleHandlerPtr handler)
    {
        const int reqID = generateRequestID();
        auto tran = prepareTransaction(ApiCommand::saveVideowall, videowall);
        m_queryProcessor->processUpdateAsync(tran, [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
        return reqID;
    }

    template<class T>
    int QnVideowallManager<T>::remove( const QnUuid& id, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        auto tran = prepareTransaction(ApiCommand::removeVideowall, id);
        m_queryProcessor->processUpdateAsync(tran, [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
        return reqID;
    }

    template<class T>
    int QnVideowallManager<T>::sendControlMessage(const ec2::ApiVideowallControlMessageData& message, impl::SimpleHandlerPtr handler)
    {
        const int reqID = generateRequestID();
        auto tran = prepareTransaction(ApiCommand::videowallControl, message);
        m_queryProcessor->processUpdateAsync(tran, [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
        return reqID;
    }

    template<class T>
    QnTransaction<ApiVideowallData> QnVideowallManager<T>::prepareTransaction(ApiCommand::Value command, const ec2::ApiVideowallData& videowall)
    {
        return QnTransaction<ApiVideowallData>(command, videowall);
    }

    template<class T>
    QnTransaction<ApiIdData> QnVideowallManager<T>::prepareTransaction(ApiCommand::Value command, const QnUuid &id)
    {
        QnTransaction<ApiIdData> tran(command);
        tran.params.id = id;
        return tran;
    }

    template<class T>
    QnTransaction<ApiVideowallControlMessageData> QnVideowallManager<T>::prepareTransaction(ApiCommand::Value command, const ec2::ApiVideowallControlMessageData& message)
    {
        return QnTransaction<ApiVideowallControlMessageData>(command, message);
    }

    template class QnVideowallManager<ServerQueryProcessor>;
    template class QnVideowallManager<FixedUrlClientQueryProcessor>;

}
