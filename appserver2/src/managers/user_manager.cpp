#include "user_manager.h"

#include "fixed_url_client_query_processor.h"
#include "server_query_processor.h"

namespace ec2
{
    QnUserNotificationManager::QnUserNotificationManager()
    {}

    void QnUserNotificationManager::triggerNotification(const QnTransaction<ApiUserData>& tran)
    {
        assert(tran.command == ApiCommand::saveUser);
        emit addedOrUpdated(tran.params);
    }

    void QnUserNotificationManager::triggerNotification(const QnTransaction<ApiIdData>& tran)
    {
        assert(tran.command == ApiCommand::removeUser);
        emit removed(tran.params.id);
    }


    template<class QueryProcessorType>
    QnUserManager<QueryProcessorType>::QnUserManager( QueryProcessorType* const queryProcessor)
    :
        QnUserNotificationManager(),
        m_queryProcessor( queryProcessor )
    {
    }

    template<class QueryProcessorType>
    int QnUserManager<QueryProcessorType>::getUsers(impl::GetUsersHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        auto queryDoneHandler = [reqID, handler]( ErrorCode errorCode, const ApiUserDataList& users)
        {
            handler->done( reqID, errorCode, users);
        };
        m_queryProcessor->template processQueryAsync<std::nullptr_t, ApiUserDataList, decltype(queryDoneHandler)>
            ( ApiCommand::getUsers, nullptr, queryDoneHandler);
        return reqID;
    }

    template<class QueryProcessorType>
    void callSaveUserAsync(
        QueryProcessorType* const queryProcessor,
        QnTransaction<ApiUserData>& tran,
        impl::SimpleHandlerPtr handler,
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
        impl::SimpleHandlerPtr handler,
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

    template<class QueryProcessorType>
    int QnUserManager<QueryProcessorType>::save( const ec2::ApiUserData& user, const QString& newPassword, impl::SimpleHandlerPtr handler )
    {
        Q_ASSERT_X(!user.id.isNull(), Q_FUNC_INFO, "User id must be set before saving");

        const int reqID = generateRequestID();
        auto tran = prepareTransaction( ApiCommand::saveUser, user);
        callSaveUserAsync( m_queryProcessor, tran, handler, reqID, user, newPassword);
        return reqID;
    }

    template<class QueryProcessorType>
    int QnUserManager<QueryProcessorType>::remove( const QnUuid& id, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        auto tran = prepareTransaction( ApiCommand::removeUser, id );
        m_queryProcessor->processUpdateAsync(tran, [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
        return reqID;
    }

    template<class QueryProcessorType>
    QnTransaction<ApiUserData> QnUserManager<QueryProcessorType>::prepareTransaction( ApiCommand::Value command, const ec2::ApiUserData& user )
    {
        return QnTransaction<ApiUserData>(command, user);
    }

    template<class QueryProcessorType>
    QnTransaction<ApiIdData> QnUserManager<QueryProcessorType>::prepareTransaction( ApiCommand::Value command, const QnUuid& id )
    {
        QnTransaction<ApiIdData> tran(command);
        tran.params.id = id;
        return tran;
    }

    template class QnUserManager<ServerQueryProcessor>;
    template class QnUserManager<FixedUrlClientQueryProcessor>;
}
