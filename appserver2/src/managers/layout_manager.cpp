#include "layout_manager.h"

#include "fixed_url_client_query_processor.h"
#include "server_query_processor.h"

namespace ec2
{

    void QnLayoutNotificationManager::triggerNotification(const QnTransaction<ApiIdData>& tran)
    {
        NX_ASSERT(tran.command == ApiCommand::removeLayout);
        emit removed(QnUuid(tran.params.id));
    }

    void QnLayoutNotificationManager::triggerNotification(const QnTransaction<ApiLayoutData>& tran)
    {
        NX_ASSERT(tran.command == ApiCommand::saveLayout);
        emit addedOrUpdated(tran.params);
    }

    void QnLayoutNotificationManager::triggerNotification(const QnTransaction<ApiLayoutDataList>& tran)
    {
        NX_ASSERT(tran.command == ApiCommand::saveLayouts);
        for (const ApiLayoutData& layout : tran.params)
            emit addedOrUpdated(layout);
    }


    template<typename QueryProcessorType>
    QnLayoutManager<QueryProcessorType>::QnLayoutManager(QueryProcessorType* const queryProcessor,
                                                         const Qn::UserAccessData &userAccessData)
        : m_queryProcessor(queryProcessor),
          m_userAccessData(userAccessData)
    {}

    template<class QueryProcessorType>
    int QnLayoutManager<QueryProcessorType>::getLayouts( impl::GetLayoutsHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        auto queryDoneHandler = [reqID, handler]( ErrorCode errorCode, const ApiLayoutDataList& layouts)
        {
            handler->done( reqID, errorCode, layouts);
        };
        m_queryProcessor->getAccess(m_userAccessData).template processQueryAsync<std::nullptr_t, ApiLayoutDataList, decltype(queryDoneHandler)>
            ( ApiCommand::getLayouts, nullptr, queryDoneHandler );
        return reqID;
    }

    template<class QueryProcessorType>
    int QnLayoutManager<QueryProcessorType>::save(const ec2::ApiLayoutData& layout, impl::SimpleHandlerPtr handler)
    {
        const int reqID = generateRequestID();
        QnTransaction<ApiLayoutData> tran(ApiCommand::saveLayout, layout );
        m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(tran, [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
        return reqID;
    }

    template<class QueryProcessorType>
    int QnLayoutManager<QueryProcessorType>::remove( const QnUuid& id, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        QnTransaction<ApiIdData> tran( ApiCommand::removeLayout, id );
        m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(tran, [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
        return reqID;
    }

    template class QnLayoutManager<FixedUrlClientQueryProcessor>;
    template class QnLayoutManager<ServerQueryProcessorAccess>;
}
