#pragma once

#include <transaction/transaction.h>
#include <nx_ec/managers/abstract_user_manager.h>
#include <api/app_server_connection.h>

namespace ec2 {

template<class QueryProcessorType>
class QnUserManager: public AbstractUserManager
{
public:
    QnUserManager(QueryProcessorType* const queryProcessor, const Qn::UserAccessData &userAccessData);


    virtual int getUsers(impl::GetUsersHandlerPtr handler) override;
    virtual int save(const ec2::ApiUserData& user, const QString& newPassword,
        impl::SimpleHandlerPtr handler) override;
    virtual int save(const ec2::ApiUserDataList& users,
        impl::SimpleHandlerPtr handler) override;
    virtual int remove(const QnUuid& id, impl::SimpleHandlerPtr handler) override;

    virtual int getUserRoles(impl::GetUserRolesHandlerPtr handler) override;
    virtual int saveUserRole(const ec2::ApiUserRoleData& userRole,
        impl::SimpleHandlerPtr handler) override;
    virtual int removeUserRole(const QnUuid& id, impl::SimpleHandlerPtr handler) override;

    virtual int getAccessRights(impl::GetAccessRightsHandlerPtr handler) override;
    virtual int setAccessRights(const ec2::ApiAccessRightsData& data,
        impl::SimpleHandlerPtr handler) override;
private:
    QueryProcessorType* const m_queryProcessor;
    Qn::UserAccessData m_userAccessData;
};

template<class QueryProcessorType>
QnUserManager<QueryProcessorType>::QnUserManager(QueryProcessorType* const queryProcessor,
    const Qn::UserAccessData& userAccessData)
    :
    m_queryProcessor(queryProcessor),
    m_userAccessData(userAccessData)
{
}

template<class QueryProcessorType>
int QnUserManager<QueryProcessorType>::getUsers(impl::GetUsersHandlerPtr handler)
{
    const int reqID = generateRequestID();

    auto queryDoneHandler =
        [reqID, handler](ErrorCode errorCode, const ApiUserDataList& users)
        {
            handler->done(reqID, errorCode, users);
        };
    m_queryProcessor->getAccess(m_userAccessData)
        .template processQueryAsync<QnUuid, ApiUserDataList, decltype(queryDoneHandler)>
        (ApiCommand::getUsers, QnUuid(), queryDoneHandler);
    return reqID;
}

template<class QueryProcessorType>
void callSaveUserAsync(
    QueryProcessorType* const queryProcessor,
    const Qn::UserAccessData& userAccessData,
    impl::SimpleHandlerPtr handler,
    const int reqID,
    const ec2::ApiUserData& user,
    const QString& newPassword)
{
    //after successfull call completion users.front()->getPassword() is empty, so saving it here
    queryProcessor->getAccess(userAccessData).processUpdateAsync(
        ApiCommand::saveUser, user,
        [queryProcessor, handler, reqID, user, newPassword](ec2::ErrorCode errorCode)
        {
            if (errorCode == ec2::ErrorCode::ok
                && queryProcessor->userName() == user.name
                && !newPassword.isEmpty())
            {
                if (auto connection = QnAppServerConnectionFactory::ec2Connection())
                {
                    auto url = connection->connectionInfo().ecUrl;
                    url.setPassword(newPassword);
                    connection->updateConnectionUrl(url);
                }
            }
            handler->done(reqID, errorCode);
        });
}

template<class QueryProcessorType>
int QnUserManager<QueryProcessorType>::save(
    const ec2::ApiUserData& user,
    const QString& newPassword,
    impl::SimpleHandlerPtr handler)
{
    NX_ASSERT(!user.id.isNull(), Q_FUNC_INFO, "User id must be set before saving");

    const int reqID = generateRequestID();
    callSaveUserAsync(m_queryProcessor, m_userAccessData, handler, reqID, user, newPassword);
    return reqID;
}

template<class QueryProcessorType>
int QnUserManager<QueryProcessorType>::save(
    const ec2::ApiUserDataList& users,
    impl::SimpleHandlerPtr handler)
{
    const int reqID = generateRequestID();
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
        ApiCommand::saveUsers, users,
        [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
    return reqID;
}

template<class QueryProcessorType>
int QnUserManager<QueryProcessorType>::remove(const QnUuid& id, impl::SimpleHandlerPtr handler)
{
    const int reqID = generateRequestID();
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
        ApiCommand::removeUser, nx::vms::api::IdData(id),
        [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
    return reqID;
}

template<class QueryProcessorType>
int ec2::QnUserManager<QueryProcessorType>::getUserRoles(impl::GetUserRolesHandlerPtr handler)
{
    const int reqID = generateRequestID();
    auto queryDoneHandler =
        [reqID, handler](ErrorCode errorCode, const ApiUserRoleDataList& result)
        {
            handler->done(reqID, errorCode, result);
        };
    m_queryProcessor->getAccess(m_userAccessData)
        .template processQueryAsync<QnUuid, ApiUserRoleDataList, decltype(queryDoneHandler)>
        (ApiCommand::getUserRoles, QnUuid(), queryDoneHandler);
    return reqID;
}

template<class QueryProcessorType>
int ec2::QnUserManager<QueryProcessorType>::saveUserRole(const ec2::ApiUserRoleData& userRole,
    impl::SimpleHandlerPtr handler)
{
    const int reqID = generateRequestID();
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
        ApiCommand::saveUserRole, userRole,
        [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
    return reqID;
}

template<class QueryProcessorType>
int ec2::QnUserManager<QueryProcessorType>::removeUserRole(const QnUuid& id,
    impl::SimpleHandlerPtr handler)
{
    const int reqID = generateRequestID();
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
        ApiCommand::removeUserRole, nx::vms::api::IdData(id),
        [handler, reqID](ec2::ErrorCode errorCode)
    {
        handler->done(reqID, errorCode);
    });
    return reqID;
}

template<class QueryProcessorType>
int QnUserManager<QueryProcessorType>::getAccessRights(impl::GetAccessRightsHandlerPtr handler)
{
    const int reqID = generateRequestID();

    auto queryDoneHandler =
        [reqID, handler](ErrorCode errorCode, const ApiAccessRightsDataList& result)
        {
            handler->done(reqID, errorCode, result);
        };
    m_queryProcessor->getAccess(m_userAccessData)
        .template processQueryAsync<std::nullptr_t, ApiAccessRightsDataList, decltype(queryDoneHandler)>
        (ApiCommand::getAccessRights, nullptr, queryDoneHandler);
    return reqID;
}

template<class QueryProcessorType>
int QnUserManager<QueryProcessorType>::setAccessRights(const ec2::ApiAccessRightsData& data,
    impl::SimpleHandlerPtr handler)
{
    const int reqID = generateRequestID();
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
        ApiCommand::setAccessRights, data,
        [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
    return reqID;
}

} // namespace ec2
