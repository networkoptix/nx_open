#include "layout_manager.h"

#include "fixed_url_client_query_processor.h"
#include "server_query_processor.h"

namespace ec2
{
    template<class QueryProcessorType>
    QnLayoutManager<QueryProcessorType>::QnLayoutManager( QueryProcessorType* const queryProcessor )
    :
        QnLayoutNotificationManager( ),
        m_queryProcessor( queryProcessor )
    {
    }

    template<class QueryProcessorType>
    int QnLayoutManager<QueryProcessorType>::getLayouts( impl::GetLayoutsHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        auto queryDoneHandler = [reqID, handler]( ErrorCode errorCode, const ApiLayoutDataList& layouts)
        {
            handler->done( reqID, errorCode, layouts);
        };
        m_queryProcessor->template processQueryAsync<std::nullptr_t, ApiLayoutDataList, decltype(queryDoneHandler)>
            ( ApiCommand::getLayouts, nullptr, queryDoneHandler );
        return reqID;
    }

    template<class QueryProcessorType>
    int QnLayoutManager<QueryProcessorType>::save(const ec2::ApiLayoutData& layout, impl::SimpleHandlerPtr handler)
    {
        const int reqID = generateRequestID();
        auto tran = prepareTransaction(ApiCommand::saveLayout, layout );
        m_queryProcessor->processUpdateAsync(tran, [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
        return reqID;
    }

    template<class QueryProcessorType>
    int QnLayoutManager<QueryProcessorType>::remove( const QnUuid& id, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        auto tran = prepareTransaction( ApiCommand::removeLayout, id );
        m_queryProcessor->processUpdateAsync(tran, [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
        return reqID;
    }

    template<class T>
    QnTransaction<ApiIdData> QnLayoutManager<T>::prepareTransaction( ApiCommand::Value command, const QnUuid& id )
    {
        QnTransaction<ApiIdData> tran(command);
        tran.params.id = id;
        return tran;
    }

    template<class T>
    QnTransaction<ApiLayoutData> QnLayoutManager<T>::prepareTransaction( ApiCommand::Value command, const ec2::ApiLayoutData& layout )
    {
        return QnTransaction<ApiLayoutData>(command, layout);
    }

    template class QnLayoutManager<FixedUrlClientQueryProcessor>;
    template class QnLayoutManager<ServerQueryProcessor>;
}
