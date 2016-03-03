#include "webpage_manager.h"

#include <functional>

#include <QtConcurrent/QtConcurrent>

#include "fixed_url_client_query_processor.h"
#include "server_query_processor.h"

#include <core/resource/webpage_resource.h>

#include <database/db_manager.h>

#include <nx_ec/data/api_conversion_functions.h>

#include <transaction/transaction_log.h>


namespace ec2
{
    QnWebPageNotificationManager::QnWebPageNotificationManager()
    {}

    void QnWebPageNotificationManager::triggerNotification(const QnTransaction<ApiWebPageData> &tran)
    {
        assert(tran.command == ApiCommand::saveWebPage);
        QnWebPageResourcePtr webPage(new QnWebPageResource());
        fromApiToResource(tran.params, webPage);
        emit addedOrUpdated( webPage );
    }

    void QnWebPageNotificationManager::triggerNotification(const QnTransaction<ApiIdData> &tran)
    {
        assert(tran.command == ApiCommand::removeWebPage );
        emit removed( QnUuid(tran.params.id) );
    }



    template<class QueryProcessorType>
    QnWebPageManager<QueryProcessorType>::QnWebPageManager(QueryProcessorType* const queryProcessor)
        : QnWebPageNotificationManager()
        , m_queryProcessor(queryProcessor)
    {}


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
    int QnWebPageManager<T>::save( const QnWebPageResourcePtr& webPage, impl::AddWebPageHandlerPtr handler )
    {
        //preparing output data
        QnWebPageResourceList webPages;
        webPages.push_back(webPage);

        const int reqID = generateRequestID();
        auto tran = prepareTransaction( ApiCommand::saveWebPage, webPage );
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


