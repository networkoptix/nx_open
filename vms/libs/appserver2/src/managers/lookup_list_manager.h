// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/rest/user_access_data.h>
#include <nx/vms/api/data/lookup_list_data.h>
#include <nx_ec/managers/abstract_lookup_list_manager.h>
#include <transaction/transaction.h>

namespace ec2 {

template<class QueryProcessorType>
class LookupListManager: public AbstractLookupListManager
{
public:
    LookupListManager(QueryProcessorType* queryProcessor, const nx::network::rest::UserSession& userSession);

    virtual int getLookupLists(
        Handler<nx::vms::api::LookupListDataList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int save(
        const nx::vms::api::LookupListData& data,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int remove(
        const nx::Uuid& id,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

private:
    decltype(auto) processor() { return m_queryProcessor->getAccess(m_userSession); }

private:
    QueryProcessorType* const m_queryProcessor;
    nx::network::rest::UserSession m_userSession;
};

template<typename QueryProcessorType>
LookupListManager<QueryProcessorType>::LookupListManager(
    QueryProcessorType* queryProcessor, const nx::network::rest::UserSession& userSession)
    :
    m_queryProcessor(queryProcessor),
    m_userSession(userSession)
{
}

template<class QueryProcessorType>
int LookupListManager<QueryProcessorType>::getLookupLists(
    Handler<nx::vms::api::LookupListDataList> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().template processQueryAsync<const nx::Uuid&, nx::vms::api::LookupListDataList>(
        ApiCommand::getLookupLists,
        nx::Uuid(),
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class QueryProcessorType>
int LookupListManager<QueryProcessorType>::save(
    const nx::vms::api::LookupListData& data,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::saveLookupList,
        data,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class QueryProcessorType>
int LookupListManager<QueryProcessorType>::remove(
    const nx::Uuid& id,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::removeLookupList,
        nx::vms::api::IdData(id),
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

} // namespace ec2
