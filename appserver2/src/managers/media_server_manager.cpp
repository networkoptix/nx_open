
#include "media_server_manager.h"

#include <functional>

#include <QtConcurrent>

#include "fixed_url_client_query_processor.h"
#include "database/db_manager.h"
#include "transaction/transaction_log.h"
#include "server_query_processor.h"
#include "core/resource/media_server_resource.h"


using namespace ec2;

namespace ec2
{
    template<class QueryProcessorType>
    QnMediaServerManager<QueryProcessorType>::QnMediaServerManager( QueryProcessorType* const queryProcessor, const ResourceContext& resCtx )
    :
        m_queryProcessor( queryProcessor ),
        m_resCtx( resCtx )
    {
    }

    template<class T>
    int QnMediaServerManager<T>::getServers( impl::GetServersHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        auto queryDoneHandler = [reqID, handler, this]( ErrorCode errorCode, const ApiMediaServerDataList& servers) {
            QnMediaServerResourceList outData;
            if( errorCode == ErrorCode::ok )
                fromApiToResourceList(servers, outData, m_resCtx);
            handler->done( reqID, errorCode, outData);
        };
        m_queryProcessor->template processQueryAsync<std::nullptr_t, ApiMediaServerDataList, decltype(queryDoneHandler)> (
            ApiCommand::getMediaServers, nullptr, queryDoneHandler);
        return reqID;
    }

    template<class T>
    int QnMediaServerManager<T>::save( const QnMediaServerResourcePtr& resource, impl::SaveServerHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        if (resource->getId().isNull())
            resource->setId( QnId::createUuid());

        QnAbstractStorageResourceList storages = resource->getStorages();
        for (int i = 0; i < storages.size(); ++i)
        {
            if (storages[i]->getId().isNull())
                storages[i]->setId(QnId::createUuid());
        }
        resource->setStorages(storages);

        //performing request
        auto tran = prepareTransaction( ApiCommand::saveMediaServer, resource );

        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( std::mem_fn( &impl::SaveServerHandler::done ), handler, reqID, _1, resource ) );

        return reqID;
    }

    template<class T>
    int QnMediaServerManager<T>::remove( const QnId& id, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        auto tran = prepareTransaction( ApiCommand::removeMediaServer, id );
        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( std::mem_fn( &impl::SimpleHandler::done ), handler, reqID, _1 ) );
        return reqID;
    }

    template<class T>
    QnTransaction<ApiMediaServerData> QnMediaServerManager<T>::prepareTransaction( ApiCommand::Value command, const QnMediaServerResourcePtr& resource )
    {
        QnTransaction<ApiMediaServerData> tran(command, true);
        fromResourceToApi(resource, tran.params);
        return tran;
    }

    template<class T>
    QnTransaction<ApiIdData> QnMediaServerManager<T>::prepareTransaction( ApiCommand::Value command, const QnId& id )
    {
        QnTransaction<ApiIdData> tran(command, true);
        tran.params.id = id;
        return tran;
    }


    template class QnMediaServerManager<ServerQueryProcessor>;
    template class QnMediaServerManager<FixedUrlClientQueryProcessor>;
}
