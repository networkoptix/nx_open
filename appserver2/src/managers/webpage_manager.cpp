#include "webpage_manager.h"

#include <functional>

#include <QtConcurrent/QtConcurrent>

#include "fixed_url_client_query_processor.h"
#include "database/db_manager.h"
#include "transaction/transaction_log.h"
#include "server_query_processor.h"


namespace ec2
{
    template<class QueryProcessorType>
    QnWebPageManager<QueryProcessorType>::QnWebPageManager( QueryProcessorType* const queryProcessor, const ResourceContext& resCtx )
        :
        QnWebPageNotificationManager( resCtx ),
        m_queryProcessor( queryProcessor )
    {
    }


    template<class T>
    int QnWebPageManager<T>::getWebPages( impl::GetWebPagesHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        auto queryDoneHandler = [reqID, handler]( ErrorCode errorCode, const ApiWebPageDataList& webPages) {
            QnWebPageResourceList outData;
            if( errorCode == ErrorCode::ok )
                fromApiToResourceList(webPages, outData);
            handler->done( reqID, errorCode, outData );
        };
        m_queryProcessor->template processQueryAsync<std::nullptr_t, ApiWebPageDataList, decltype(queryDoneHandler)> ( ApiCommand::getWebPages, nullptr, queryDoneHandler);
        return reqID;
    }

    template<class T>
    int QnWebPageManager<T>::save( const QnWebPageResourcePtr& resource, impl::AddWebPageHandlerPtr handler )
    {
        //preparing output data
        QnWebPageResourceList webPages;
        if (resource->getId().isNull()) {
            resource->setId(QnUuid::createUuid());
        }
        webPages.push_back( resource );

        const int reqID = generateRequestID();
        auto tran = prepareTransaction( ApiCommand::saveWebPage, resource );
        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( &impl::AddWebPageHandler::done, handler, reqID, _1, webPages ) );
        return reqID;
    }

    template<class T>
    int QnWebPageManager<T>::remove( const QnUuid& id, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        auto tran = prepareTransaction( ApiCommand::removeWebPage, id );
        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( &impl::SimpleHandler::done, handler, reqID, _1 ) );
        return reqID;
    }

    template<class T>
    QnTransaction<ApiWebPageData> QnWebPageManager<T>::prepareTransaction(ApiCommand::Value command, const QnWebPageResourcePtr &resource)
    {
        QnTransaction<ApiWebPageData> tran(command);
        fromResourceToApi( resource, tran.params );
        return tran;
    }

    template<class T>
    QnTransaction<ApiIdData> QnWebPageManager<T>::prepareTransaction(ApiCommand::Value command, const QnUuid &id)
    {
        QnTransaction<ApiIdData> tran(command);
        tran.params.id = id;
        return tran;
    }

    template class QnWebPageManager<ServerQueryProcessor>;
    template class QnWebPageManager<FixedUrlClientQueryProcessor>;

}


