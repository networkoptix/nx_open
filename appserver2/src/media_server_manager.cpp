
#include "media_server_manager.h"

#include <functional>

#include <QtConcurrent>

#include "client_query_processor.h"
#include "cluster/cluster_manager.h"
#include "database/db_manager.h"
#include "transaction/transaction_log.h"
#include "server_query_processor.h"


using namespace ec2;

namespace ec2
{
    template<class QueryProcessorType>
    QnMediaServerManager<QueryProcessorType>::QnMediaServerManager( QueryProcessorType* const queryProcessor, QSharedPointer<QnResourceFactory> factory)
    :
        m_queryProcessor( queryProcessor ),
        m_resourcefactory(factory)
    {
    }

    template<class T>
    ReqID QnMediaServerManager<T>::getServers( impl::GetServersHandlerPtr handler )
    {
        auto queryDoneHandler = [handler, this]( ErrorCode errorCode, const ApiMediaServerDataList& servers) {
            QnMediaServerResourceList outData;
            if( errorCode == ErrorCode::ok )
                servers.toResourceList(outData, m_resourcefactory.data());
            handler->done( errorCode, outData);
        };
        m_queryProcessor->processQueryAsync<nullptr_t, ApiMediaServerDataList, decltype(queryDoneHandler)> (nullptr, queryDoneHandler);
        return INVALID_REQ_ID;
    }

    template<class T>
    ReqID QnMediaServerManager<T>::save( const QnMediaServerResourcePtr& resource, impl::SimpleHandlerPtr handler )
    {
        //create transaction
        const QnTransaction<ApiMediaServerData>& tran = prepareTransaction( ec2::saveMediaServer, resource );

        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( std::mem_fn( &impl::SimpleHandler::done ), handler, _1 ) );

        return INVALID_REQ_ID;
    }

    template<class T>
    ReqID QnMediaServerManager<T>::saveServer( const QnMediaServerResourcePtr&, impl::SaveServerHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }

    template<class T>
    ReqID QnMediaServerManager<T>::remove( const QnMediaServerResourcePtr& resource, impl::SimpleHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }

    template<class T>
    QnTransaction<ApiMediaServerData> QnMediaServerManager<T>::prepareTransaction( ApiCommand command, const QnMediaServerResourcePtr& resource )
    {
        QnTransaction<ApiMediaServerData> tran;
        tran.createNewID();
        tran.command = command;
        tran.persistent = true;
        //TODO/IMPL
        return tran;
    }



    template class QnMediaServerManager<ServerQueryProcessor>;
    template class QnMediaServerManager<ClientQueryProcessor>;
}
