
#include "user_manager.h"

#include <functional>

#include <QtConcurrent>

#include "client_query_processor.h"
#include "cluster/cluster_manager.h"
#include "database/db_manager.h"
#include "transaction/transaction_log.h"
#include "server_query_processor.h"


namespace ec2
{
    template<class QueryProcessorType>
    QnUserManager<QueryProcessorType>::QnUserManager( QueryProcessorType* const queryProcessor, const ResourceContext& resCtx )
        :
        m_queryProcessor( queryProcessor ),
        m_resCtx(resCtx)
    {
    }


    template<class T>
    ReqID QnUserManager<T>::getUsers( impl::GetUsersHandlerPtr handler )
    {
        auto queryDoneHandler = [handler]( ErrorCode errorCode, const ApiUserDataList& users) {
            QnUserResourceList outData;
            if( errorCode == ErrorCode::ok )
                users.toResourceList(outData);
            handler->done( errorCode, outData);
        };
        m_queryProcessor->processQueryAsync<nullptr_t, ApiUserDataList, decltype(queryDoneHandler)> ( ApiCommand::getUserList, nullptr, queryDoneHandler);
        return INVALID_REQ_ID;
    }

    template<class T>
    ReqID QnUserManager<T>::save( const QnUserResourcePtr& resource, impl::SimpleHandlerPtr handler )
    {
        Q_ASSERT_X(0, Q_FUNC_INFO, "Implement me!!!");
        return INVALID_REQ_ID;
    }

    template<class T>
    ReqID QnUserManager<T>::remove( const QnUserResourcePtr& resource, impl::SimpleHandlerPtr handler )
    {
        Q_ASSERT_X(0, Q_FUNC_INFO, "Implement me!!!");
        return INVALID_REQ_ID;
    }

    template<class T>
    QnTransaction<ApiUserData> QnUserManager<T>::prepareTransaction( ApiCommand::Value command, const QnUserResourcePtr& resource )
    {
        QnTransaction<ApiUserData> tran;
        tran.createNewID();
        tran.command = command;
        tran.persistent = true;
        //TODO/IMPL
        return tran;
    }

    template class QnUserManager<ServerQueryProcessor>;
    template class QnUserManager<ClientQueryProcessor>;

}
