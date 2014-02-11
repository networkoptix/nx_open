
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
                layouts.toResourceList(outData);
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
    int QnLayoutManager<QueryProcessorType>::remove( const QnId& id, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        auto tran = prepareTransaction( ApiCommand::removeLayout, id );
        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( std::mem_fn( &impl::SimpleHandler::done ), handler, reqID, _1 ) );
        return reqID;
    }

    template<class T>
    QnTransaction<ApiIdData> QnLayoutManager<T>::prepareTransaction( ApiCommand::Value command, const QnId& id )
    {
        QnTransaction<ApiIdData> tran;
        tran.createNewID();
        tran.command = command;
        tran.persistent = true;
        tran.params.id = id;
        return tran;
    }

    template class QnLayoutManager<FixedUrlClientQueryProcessor>;
    template class QnLayoutManager<ServerQueryProcessor>;
}
