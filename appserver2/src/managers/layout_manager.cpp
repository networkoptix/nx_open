
#include "layout_manager.h"

#include <core/resource/layout_resource.h>

#include "fixed_url_client_query_processor.h"
#include "server_query_processor.h"


namespace ec2
{
    template<class QueryProcessorType>
    QnLayoutManager<QueryProcessorType>::QnLayoutManager( QueryProcessorType* const queryProcessor, const ResourceContext& resCtx )
    :
        QnLayoutNotificationManager( resCtx ),
        m_queryProcessor( queryProcessor )
    {
    }

    template<class QueryProcessorType>
    int QnLayoutManager<QueryProcessorType>::getLayouts( impl::GetLayoutsHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        auto queryDoneHandler = [reqID, handler]( ErrorCode errorCode, const ApiLayoutDataList& layouts) {
            QnLayoutResourceList outData;
            if( errorCode == ErrorCode::ok )
                fromApiToResourceList(layouts, outData);
            handler->done( reqID, errorCode, outData);
        };
        m_queryProcessor->template processQueryAsync<std::nullptr_t, ApiLayoutDataList, decltype(queryDoneHandler)>
            ( ApiCommand::getLayouts, nullptr, queryDoneHandler );
        return reqID;
    }

    template<class QueryProcessorType>
    int QnLayoutManager<QueryProcessorType>::save( const QnLayoutResourceList& layouts, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        //preparing output data
		ApiCommand::Value command = ApiCommand::saveLayouts;
        for( QnLayoutResourcePtr layout: layouts )
        {
            if( layout->getId().isNull() )
			    layout->setId( QUuid::createUuid() );
        }

        //performing request
        auto tran = prepareTransaction( command, layouts );

        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( &impl::SimpleHandler::done, handler, reqID, _1 ) );
        
        return reqID;
    }

    template<class QueryProcessorType>
    int QnLayoutManager<QueryProcessorType>::remove( const QUuid& id, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        auto tran = prepareTransaction( ApiCommand::removeLayout, id );
        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( std::mem_fn( &impl::SimpleHandler::done ), handler, reqID, _1 ) );
        return reqID;
    }

    template<class T>
    QnTransaction<ApiIdData> QnLayoutManager<T>::prepareTransaction( ApiCommand::Value command, const QUuid& id )
    {
        QnTransaction<ApiIdData> tran(command);
        tran.params.id = id;
        return tran;
    }

    template<class T>
    QnTransaction<ApiLayoutDataList> QnLayoutManager<T>::prepareTransaction( ApiCommand::Value command, const QnLayoutResourceList& layouts )
    {
        QnTransaction<ApiLayoutDataList> tran(command);
        fromResourceListToApi(layouts, tran.params);
        return tran;
    }

    template class QnLayoutManager<FixedUrlClientQueryProcessor>;
    template class QnLayoutManager<ServerQueryProcessor>;
}
