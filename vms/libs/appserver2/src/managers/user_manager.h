// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <transaction/transaction.h>
#include <nx_ec/managers/abstract_user_manager.h>

namespace ec2 {

template<class QueryProcessorType>
class QnUserManager: public AbstractUserManager
{
public:
    QnUserManager(QueryProcessorType* queryProcessor, const Qn::UserSession& userSession);

    virtual int getUsers(
        Handler<nx::vms::api::UserDataList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int save(
        const nx::vms::api::UserData& data,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int save(
        const nx::vms::api::UserDataList& dataList,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int remove(
        const QnUuid& id,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int getUserRoles(
        Handler<nx::vms::api::UserRoleDataList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int saveUserRole(
        const nx::vms::api::UserRoleData& data,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int removeUserRole(
        const QnUuid& id,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int getAccessRights(
        Handler<nx::vms::api::AccessRightsDataList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int setAccessRights(
        const nx::vms::api::AccessRightsData& data,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

private:
    decltype(auto) processor() { return m_queryProcessor->getAccess(m_userSession); }

private:
    QueryProcessorType* const m_queryProcessor;
    Qn::UserSession m_userSession;
};

template<class QueryProcessorType>
QnUserManager<QueryProcessorType>::QnUserManager(
    QueryProcessorType* queryProcessor, const Qn::UserSession& userSession)
    :
    m_queryProcessor(queryProcessor),
    m_userSession(userSession)
{
}

template<class QueryProcessorType>
int QnUserManager<QueryProcessorType>::getUsers(
    Handler<nx::vms::api::UserDataList> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    handler = handlerExecutor.bind(std::move(handler));
    const int requestId = generateRequestID();
    processor().template processQueryAsync<QnUuid, nx::vms::api::UserDataList>(
        ApiCommand::getUsers,
        QnUuid(),
        [requestId, handler](auto&&... args) { handler(requestId, std::move(args)...); });
    return requestId;
}

template<class QueryProcessorType>
int QnUserManager<QueryProcessorType>::save(
    const nx::vms::api::UserData& data,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    NX_ASSERT(!data.id.isNull(), "User id must be set before saving");

    handler = handlerExecutor.bind(std::move(handler));
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::saveUser,
        data,
        [requestId, handler](Result result) { handler(requestId, std::move(result)); });

    return requestId;
}

template<class QueryProcessorType>
int QnUserManager<QueryProcessorType>::save(
    const nx::vms::api::UserDataList& dataList,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    handler = handlerExecutor.bind(std::move(handler));
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::saveUsers,
        dataList,
        [requestId, handler](auto&&... args) { handler(requestId, std::move(args)...); });
    return requestId;
}

template<class QueryProcessorType>
int QnUserManager<QueryProcessorType>::remove(
    const QnUuid& id,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    handler = handlerExecutor.bind(std::move(handler));
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::removeUser,
        nx::vms::api::IdData(id),
        [requestId, handler](auto&&... args) { handler(requestId, std::move(args)...); });
    return requestId;
}

template<class QueryProcessorType>
int QnUserManager<QueryProcessorType>::getUserRoles(
    Handler<nx::vms::api::UserRoleDataList> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    handler = handlerExecutor.bind(std::move(handler));
    const int requestId = generateRequestID();
    processor().template processQueryAsync<QnUuid, nx::vms::api::UserRoleDataList>(
        ApiCommand::getUserRoles,
        QnUuid(),
        [requestId, handler](auto&&... args) { handler(requestId, std::move(args)...); });
    return requestId;
}

template<class QueryProcessorType>
int QnUserManager<QueryProcessorType>::saveUserRole(
    const nx::vms::api::UserRoleData& data,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    handler = handlerExecutor.bind(std::move(handler));
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::saveUserRole,
        data,
        [requestId, handler](auto&&... args) { handler(requestId, std::move(args)...); });
    return requestId;
}

template<class QueryProcessorType>
int QnUserManager<QueryProcessorType>::removeUserRole(
    const QnUuid& id,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    handler = handlerExecutor.bind(std::move(handler));
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::removeUserRole,
        nx::vms::api::IdData(id),
        [requestId, handler](auto&&... args) { handler(requestId, std::move(args)...); });
    return requestId;
}

template<class QueryProcessorType>
int QnUserManager<QueryProcessorType>::getAccessRights(
    Handler<nx::vms::api::AccessRightsDataList> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    handler = handlerExecutor.bind(std::move(handler));
    const int requestId = generateRequestID();
    processor().template processQueryAsync<QnUuid, nx::vms::api::AccessRightsDataList>(
        ApiCommand::getAccessRights,
        QnUuid(),
        [requestId, handler](auto&&... args) { handler(requestId, std::move(args)...); });
    return requestId;
}

template<class QueryProcessorType>
int QnUserManager<QueryProcessorType>::setAccessRights(
    const nx::vms::api::AccessRightsData& data,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    handler = handlerExecutor.bind(std::move(handler));
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::setAccessRights,
        data,
        [requestId, handler](auto&&... args) { handler(requestId, std::move(args)...); });
    return requestId;
}

} // namespace ec2
