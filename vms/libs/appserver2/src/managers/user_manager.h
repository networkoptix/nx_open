// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx_ec/managers/abstract_user_manager.h>
#include <transaction/transaction.h>

namespace ec2 {

template<class QueryProcessorType>
class QnUserManager: public AbstractUserManager
{
public:
    QnUserManager(QueryProcessorType* queryProcessor, nx::network::rest::audit::Record auditRecord);

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
        const nx::Uuid& id,
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
        const nx::Uuid& id,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

private:
    decltype(auto) processor() { return m_queryProcessor->getAccess(m_auditRecord); }

private:
    QueryProcessorType* const m_queryProcessor;
    nx::network::rest::audit::Record m_auditRecord;
};

template<class QueryProcessorType>
QnUserManager<QueryProcessorType>::QnUserManager(
    QueryProcessorType* queryProcessor, nx::network::rest::audit::Record auditRecord)
    :
    m_queryProcessor(queryProcessor),
    m_auditRecord(std::move(auditRecord))
{
}

template<class QueryProcessorType>
int QnUserManager<QueryProcessorType>::getUsers(
    Handler<nx::vms::api::UserDataList> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().template processQueryAsync<nx::Uuid, nx::vms::api::UserDataList>(
        ApiCommand::getUsers,
        nx::Uuid(),
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class QueryProcessorType>
int QnUserManager<QueryProcessorType>::save(
    const nx::vms::api::UserData& data,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    NX_ASSERT(!data.id.isNull(), "User id must be set before saving");

    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::saveUser,
        data,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class QueryProcessorType>
int QnUserManager<QueryProcessorType>::save(
    const nx::vms::api::UserDataList& dataList,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::saveUsers,
        dataList,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class QueryProcessorType>
int QnUserManager<QueryProcessorType>::remove(
    const nx::Uuid& id,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::removeUser,
        nx::vms::api::IdData(id),
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class QueryProcessorType>
int QnUserManager<QueryProcessorType>::getUserRoles(
    Handler<nx::vms::api::UserGroupDataList> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().template processQueryAsync<nx::Uuid, nx::vms::api::UserGroupDataList>(
        ApiCommand::getUserGroups,
        nx::Uuid(),
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class QueryProcessorType>
int QnUserManager<QueryProcessorType>::saveUserRole(
    const nx::vms::api::UserGroupData& data,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::saveUserGroup,
        data,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class QueryProcessorType>
int QnUserManager<QueryProcessorType>::removeUserRole(
    const nx::Uuid& id,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::removeUserGroup,
        nx::vms::api::IdData(id),
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

} // namespace ec2
