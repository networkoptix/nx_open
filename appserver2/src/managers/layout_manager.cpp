#include "layout_manager.h"

#include "fixed_url_client_query_processor.h"
#include "server_query_processor.h"

namespace ec2
{

    void QnLayoutNotificationManager::triggerNotification(const QnTransaction<ApiIdData>& tran, NotificationSource /*source*/)
    {
        NX_ASSERT(tran.command == ApiCommand::removeLayout);
        emit removed(QnUuid(tran.params.id));
    }

    void QnLayoutNotificationManager::triggerNotification(const QnTransaction<ApiLayoutData>& tran, NotificationSource source)
    {
        NX_ASSERT(tran.command == ApiCommand::saveLayout);
        emit addedOrUpdated(tran.params, source);
    }

    void QnLayoutNotificationManager::triggerNotification(const QnTransaction<ApiLayoutDataList>& tran, NotificationSource source)
    {
        NX_ASSERT(tran.command == ApiCommand::saveLayouts);
        for (const ApiLayoutData& layout : tran.params)
            emit addedOrUpdated(layout, source);
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
        m_queryProcessor->getAccess(m_userAccessData).template processQueryAsync<const QnUuid&, ApiLayoutDataList, decltype(queryDoneHandler)>
            ( ApiCommand::getLayouts, QnUuid(), queryDoneHandler );
        return reqID;
    }

    template<class QueryProcessorType>
    int QnLayoutManager<QueryProcessorType>::save(const ec2::ApiLayoutData& layout, impl::SimpleHandlerPtr handler)
    {
        const int reqID = generateRequestID();
        m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
            ApiCommand::saveLayout,
            layout,
            [handler, reqID](ec2::ErrorCode errorCode)
            {
                handler->done(reqID, errorCode);
            });
        return reqID;
    }

    template<class QueryProcessorType>
    int QnLayoutManager<QueryProcessorType>::remove( const QnUuid& id, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
            ApiCommand::removeLayout, ApiIdData(id),
            [handler, reqID](ec2::ErrorCode errorCode)
            {
                handler->done(reqID, errorCode);
            });
        return reqID;
    }

    template class QnLayoutManager<FixedUrlClientQueryProcessor>;
    template class QnLayoutManager<ServerQueryProcessorAccess>;
}
