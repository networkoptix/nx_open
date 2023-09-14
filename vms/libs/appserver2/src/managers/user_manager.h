// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx_ec/managers/abstract_user_manager.h>
#include <transaction/transaction.h>

namespace ec2 {

template<class QueryProcessorType>
class QnUserManager: public AbstractUserManager
{
public:
    QnUserManager(QueryProcessorType* queryProcessor, Qn::UserSession userSession);

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

    virtual int remove(
        const nx::vms::api::IdDataList& ids,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int getUserRoles(
        Handler<nx::vms::api::UserGroupDataList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int saveUserRole(
        const nx::vms::api::UserGroupData& data,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int removeUserRole(
        const QnUuid& id,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int removeUserRoles(
        const nx::vms::api::IdDataList& groupList,
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
    QueryProcessorType* queryProcessor, Qn::UserSession userSession)
    :
    m_queryProcessor(queryProcessor),
    m_userSession(std::move(userSession))
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
int QnUserManager<QueryProcessorType>::remove(
    const nx::vms::api::IdDataList& userList,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    handler = handlerExecutor.bind(std::move(handler));
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::removeResources,
        userList,
        [requestId, handler](auto&&... args) { handler(requestId, std::move(args)...); });
    return requestId;
}

template<class QueryProcessorType>
int QnUserManager<QueryProcessorType>::getUserRoles(
    Handler<nx::vms::api::UserGroupDataList> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    handler = handlerExecutor.bind(std::move(handler));
    const int requestId = generateRequestID();
    processor().template processQueryAsync<QnUuid, nx::vms::api::UserGroupDataList>(
        ApiCommand::getUserGroups,
        QnUuid(),
        [requestId, handler](auto&&... args) { handler(requestId, std::move(args)...); });
    return requestId;
}

template<class QueryProcessorType>
int QnUserManager<QueryProcessorType>::saveUserRole(
    const nx::vms::api::UserGroupData& data,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    handler = handlerExecutor.bind(std::move(handler));
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::saveUserGroup,
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
        ApiCommand::removeUserGroup,
        nx::vms::api::IdData(id),
        [requestId, handler](auto&&... args) { handler(requestId, std::move(args)...); });
    return requestId;
}

template<class QueryProcessorType>
int QnUserManager<QueryProcessorType>::removeUserRoles(
    const nx::vms::api::IdDataList& userGroups,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    handler = handlerExecutor.bind(std::move(handler));
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::removeUserGroups,
        userGroups,
        [requestId, handler](auto&&... args) { handler(requestId, std::move(args)...); });
    return requestId;
}

} // namespace ec2
