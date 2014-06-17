#include "user_manager.h"

#include <functional>

#include <QtConcurrent>

#include "fixed_url_client_query_processor.h"
#include "database/db_manager.h"
#include "transaction/transaction_log.h"
#include "server_query_processor.h"
#include "nx_ec/data/api_conversion_functions.h"


namespace ec2
{
    template<class QueryProcessorType>
    QnUserManager<QueryProcessorType>::QnUserManager( QueryProcessorType* const queryProcessor, const ResourceContext& resCtx )
    :
        QnUserNotificationManager(resCtx),
        m_queryProcessor( queryProcessor )
    {
    }

    template<class T>
    int QnUserManager<T>::getUsers( impl::GetUsersHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        auto queryDoneHandler = [reqID, handler]( ErrorCode errorCode, const ApiUserDataList& users) {
            QnUserResourceList outData;
            if( errorCode == ErrorCode::ok )
                fromApiToResourceList(users, outData);
            handler->done( reqID, errorCode, outData );
        };
        m_queryProcessor->template processQueryAsync<std::nullptr_t, ApiUserDataList, decltype(queryDoneHandler)> ( ApiCommand::getUserList, nullptr, queryDoneHandler);
        return reqID;
    }

    template<class QueryProcessorType>
    void callSaveUserAsync(
        QueryProcessorType* const queryProcessor,
        QnTransaction<ApiUserData>& tran,
        impl::AddUserHandlerPtr handler,
        const int reqID,
        QnUserResourceList users )
    {
        using namespace std::placeholders;
        queryProcessor->processUpdateAsync( tran, std::bind( &impl::AddUserHandler::done, handler, reqID, _1, users ) );
    }

    template<>
    void callSaveUserAsync<FixedUrlClientQueryProcessor>(
        FixedUrlClientQueryProcessor* const queryProcessor,
        QnTransaction<ApiUserData>& tran,
        impl::AddUserHandlerPtr handler,
        const int reqID,
        QnUserResourceList users )
    {
        //after successfull call completion users.front()->getPassword() is empty, so saving it here
        QString newPassword = users.front()->getPassword();
        queryProcessor->processUpdateAsync( tran, 
            [queryProcessor, handler, reqID, users, newPassword]( ec2::ErrorCode errorCode ){
                if( errorCode == ec2::ErrorCode::ok && queryProcessor->userName() == users.front()->getName() )
                    queryProcessor->setPassword( newPassword );
                handler->done( reqID, errorCode, users );
            });
    }

    template<class T>
    int QnUserManager<T>::save( const QnUserResourcePtr& resource, impl::AddUserHandlerPtr handler )
    {
        //preparing output data
        QnUserResourceList users;
        if (resource->getId().isNull()) {
            resource->setId(QnId::createUuid());
        }
        users.push_back( resource );

        const int reqID = generateRequestID();
        auto tran = prepareTransaction( ApiCommand::saveUser, resource );
        callSaveUserAsync( m_queryProcessor, tran, handler, reqID, users );
        return reqID;
    }

    template<class T>
    int QnUserManager<T>::remove( const QnId& id, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        auto tran = prepareTransaction( ApiCommand::removeUser, id );
        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( &impl::SimpleHandler::done, handler, reqID, _1 ) );
        return reqID;
    }

    template<class T>
    QnTransaction<ApiUserData> QnUserManager<T>::prepareTransaction( ApiCommand::Value command, const QnUserResourcePtr& resource )
    {
        QnTransaction<ApiUserData> tran(command, true);
        fromResourceToApi(resource, tran.params);
        return tran;
    }

    template<class T>
    QnTransaction<ApiIdData> QnUserManager<T>::prepareTransaction( ApiCommand::Value command, const QnId& id )
    {
        QnTransaction<ApiIdData> tran(command, true);
        tran.params.id = id;
        return tran;
    }

    template class QnUserManager<ServerQueryProcessor>;
    template class QnUserManager<FixedUrlClientQueryProcessor>;

}
