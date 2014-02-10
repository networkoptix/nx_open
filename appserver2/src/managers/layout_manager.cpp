
#include "layout_manager.h"

#include "fixed_url_client_query_processor.h"
#include "server_query_processor.h"


namespace ec2
{
    template<class QueryProcessorType>
    QnLayoutManager<QueryProcessorType>::QnLayoutManager( QueryProcessorType* const queryProcessor, const ResourceContext& resCtx )
    :
        m_queryProcessor( queryProcessor ),
        m_resCtx( resCtx )
    {
    }

    template<class QueryProcessorType>
    int QnLayoutManager<QueryProcessorType>::getLayouts( impl::GetLayoutsHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        auto queryDoneHandler = [reqID, handler]( ErrorCode errorCode, const ApiLayoutDataList& layouts) {
            QnLayoutResourceList outData;
            if( errorCode == ErrorCode::ok )
                layouts.toLayoutList(outData);
            handler->done( reqID, errorCode, outData);
        };
        m_queryProcessor->processQueryAsync<nullptr_t, ApiLayoutDataList, decltype(queryDoneHandler)>
            ( ApiCommand::getLayoutList, nullptr, queryDoneHandler );
        return reqID;
    }

    template<class QueryProcessorType>
    int QnLayoutManager<QueryProcessorType>::save( const QnLayoutResourceList& resources, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        //TODO/IMPL
        return reqID;
    }

    template<class QueryProcessorType>
    int QnLayoutManager<QueryProcessorType>::remove( const QnLayoutResourcePtr& resource, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        //TODO/IMPL
        return reqID;
    }


    template class QnLayoutManager<FixedUrlClientQueryProcessor>;
    template class QnLayoutManager<ServerQueryProcessor>;
}
