
#include "media_server_manager.h"

#include <functional>

#include <QtConcurrent>

#include "client_query_processor.h"
#include "cluster/cluster_manager.h"
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
    ReqID QnMediaServerManager<T>::getServers( impl::GetServersHandlerPtr handler )
    {
        const ReqID reqID = generateRequestID();

        auto queryDoneHandler = [reqID, handler, this]( ErrorCode errorCode, const ApiMediaServerDataList& servers) {
            QnMediaServerResourceList outData;
            if( errorCode == ErrorCode::ok )
                servers.toResourceList(outData, m_resCtx.resFactory, m_resCtx.resTypePool);
            handler->done( reqID, errorCode, outData);
        };
        m_queryProcessor->processQueryAsync<nullptr_t, ApiMediaServerDataList, decltype(queryDoneHandler)> (
            ApiCommand::getMediaServerList, nullptr, queryDoneHandler);
        return reqID;
    }

    template<class T>
    ReqID QnMediaServerManager<T>::save( const QnMediaServerResourcePtr& resource, impl::SimpleHandlerPtr handler )
    {
        const ReqID reqID = generateRequestID();

        //create transaction
        const QnTransaction<ApiMediaServerData>& tran = prepareTransaction( ApiCommand::addMediaServer, resource );
        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( std::mem_fn( &impl::SimpleHandler::done ), handler, reqID, _1 ) );
        return reqID;
    }

    template<class T>
    ReqID QnMediaServerManager<T>::saveServer( const QnMediaServerResourcePtr& resource, impl::SaveServerHandlerPtr handler )
    {
        const ReqID reqID = generateRequestID();

        QnMediaServerResourceList serverList;
        ApiCommand::Value command = ApiCommand::updateMediaServer;
        if (!resource->getId().isValid()) {
            resource->setId(dbManager->getNextSequence());
            command = ApiCommand::addMediaServer;
        }
        serverList.push_back( resource );

        QnAbstractStorageResourceList storages = resource->getStorages();
        for (int i = 0; i < storages.size(); ++i)
        {
            if (!storages[i]->getId().isValid())
                storages[i]->setId(dbManager->getNextSequence());
        }
        resource->setStorages(storages);

        //performing request
        auto tran = prepareTransaction( command, resource );

        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( std::mem_fn( &impl::SaveServerHandler::done ), handler, reqID, _1, serverList ) );

        return reqID;
    }

    template<class T>
    ReqID QnMediaServerManager<T>::remove( const QnMediaServerResourcePtr& resource, impl::SimpleHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }

    template<class T>
    QnTransaction<ApiMediaServerData> QnMediaServerManager<T>::prepareTransaction( ApiCommand::Value command, const QnMediaServerResourcePtr& resource )
    {
        QnTransaction<ApiMediaServerData> tran;
        tran.createNewID();
        tran.command = command;
        tran.persistent = true;
        tran.params.fromResource(resource);
        return tran;
    }



    template class QnMediaServerManager<ServerQueryProcessor>;
    template class QnMediaServerManager<ClientQueryProcessor>;
}
