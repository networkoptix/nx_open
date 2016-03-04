#include "user_manager.h"

#include <functional>

#include <QtConcurrent/QtConcurrent>

#include "fixed_url_client_query_processor.h"
#include "database/db_manager.h"
#include "transaction/transaction_log.h"
#include "server_query_processor.h"
#include "nx_ec/data/api_conversion_functions.h"


namespace ec2
{
    template<class QueryProcessorType>
    QnUserManager<QueryProcessorType>::QnUserManager( QueryProcessorType* const queryProcessor)
    :
        QnUserNotificationManager(),
        m_queryProcessor( queryProcessor )
    {
    }

    template<class T>
    int QnUserManager<T>::getUsers(impl::GetUsersHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        auto queryDoneHandler = [reqID, handler]( ErrorCode errorCode, const ApiUserDataList& users) {
            handler->done( reqID, errorCode, users);
        };
        m_queryProcessor->template processQueryAsync<std::nullptr_t, ApiUserDataList, decltype(queryDoneHandler)> ( ApiCommand::getUsers, nullptr, queryDoneHandler);
        return reqID;
    }

    template<class QueryProcessorType>
    void callSaveUserAsync(
        QueryProcessorType* const queryProcessor,
        QnTransaction<ApiUserData>& tran,
        impl::AddUserHandlerPtr handler,
        const int reqID,
        const ec2::ApiUserData& user,
        const QString& newPassword)
    {
        QN_UNUSED(user, newPassword); /* Actual only for FixedUrlClientQueryProcessor implementation */
        queryProcessor->processUpdateAsync(tran, [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
    }

    template<>
    void callSaveUserAsync<FixedUrlClientQueryProcessor>(
        FixedUrlClientQueryProcessor* const queryProcessor,
        QnTransaction<ApiUserData>& tran,
        impl::AddUserHandlerPtr handler,
        const int reqID,
        const ec2::ApiUserData& user,
        const QString& newPassword)
    {
        //after successfull call completion users.front()->getPassword() is empty, so saving it here
        queryProcessor->processUpdateAsync( tran,
            [queryProcessor, handler, reqID, user, newPassword]( ec2::ErrorCode errorCode )
        {
                if( errorCode == ec2::ErrorCode::ok
                    && queryProcessor->userName() == user.name
                    && !newPassword.isEmpty()
                    )
                    queryProcessor->setPassword( newPassword );
                handler->done( reqID, errorCode );
        });
    }

    template<class T>
    int QnUserManager<T>::save( const ec2::ApiUserData& user, const QString& newPassword, impl::AddUserHandlerPtr handler )
    {
        Q_ASSERT_X(!user.id.isNull(), Q_FUNC_INFO, "User id must be set before saving");

        const int reqID = generateRequestID();
        auto tran = prepareTransaction( ApiCommand::saveUser, user);
        callSaveUserAsync( m_queryProcessor, tran, handler, reqID, user, newPassword);
        return reqID;
    }

    template<class T>
    int QnUserManager<T>::remove( const QnUuid& id, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        auto tran = prepareTransaction( ApiCommand::removeUser, id );
        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( &impl::SimpleHandler::done, handler, reqID, _1 ) );
        return reqID;
    }

    template<class T>
    QnTransaction<ApiUserData> QnUserManager<T>::prepareTransaction( ApiCommand::Value command, const ec2::ApiUserData& user )
    {
        return QnTransaction<ApiUserData>(command, user);
    }

    template<class T>
    QnTransaction<ApiIdData> QnUserManager<T>::prepareTransaction( ApiCommand::Value command, const QnUuid& id )
    {
        QnTransaction<ApiIdData> tran(command);
        tran.params.id = id;
        return tran;
    }

    template class QnUserManager<ServerQueryProcessor>;
    template class QnUserManager<FixedUrlClientQueryProcessor>;

}
