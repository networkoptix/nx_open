
#include "user_manager.h"

#include <functional>

#include <QtConcurrent>

#include "fixed_url_client_query_processor.h"
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
    int QnUserManager<T>::getUsers( impl::GetUsersHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        auto queryDoneHandler = [reqID, handler]( ErrorCode errorCode, const ApiUserDataList& users) {
            QnUserResourceList outData;
            if( errorCode == ErrorCode::ok )
                users.toResourceList(outData);
            handler->done( reqID, errorCode, outData );
        };
        m_queryProcessor->processQueryAsync<nullptr_t, ApiUserDataList, decltype(queryDoneHandler)> ( ApiCommand::getUserList, nullptr, queryDoneHandler);
        return reqID;
    }

    template<class T>
    int QnUserManager<T>::save( const QnUserResourcePtr& resource, impl::SimpleHandlerPtr handler )
    {
        Q_ASSERT_X(0, Q_FUNC_INFO, "Implement me!!!");
        return INVALID_REQ_ID;
    }

    template<class T>
    int QnUserManager<T>::remove( const QnUserResourcePtr& resource, impl::SimpleHandlerPtr handler )
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
        tran.params.fromResource( resource );
        return tran;
    }

    template class QnUserManager<ServerQueryProcessor>;
    template class QnUserManager<FixedUrlClientQueryProcessor>;

}
