// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/rest/user_access_data.h>
#include <nx/vms/api/data/showreel_data.h>
#include <nx_ec/managers/abstract_showreel_manager.h>
#include <transaction/transaction.h>

namespace ec2 {

template<class QueryProcessorType>
class ShowreelManager: public AbstractShowreelManager
{
public:
    ShowreelManager(QueryProcessorType* queryProcessor, const nx::network::rest::audit::Record& auditRecord);

    virtual int getShowreels(
        Handler<nx::vms::api::ShowreelDataList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int save(
        const nx::vms::api::ShowreelData& data,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int remove(
        const nx::Uuid& tourId,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

private:
    decltype(auto) processor() { return m_queryProcessor->getAccess(m_auditRecord); }

private:
    QueryProcessorType* const m_queryProcessor;
    nx::network::rest::audit::Record m_auditRecord;
};

template<typename QueryProcessorType>
ShowreelManager<QueryProcessorType>::ShowreelManager(
    QueryProcessorType* queryProcessor, const nx::network::rest::audit::Record& auditRecord)
    :
    m_queryProcessor(queryProcessor),
    m_auditRecord(auditRecord)
{
}

template<class QueryProcessorType>
int ShowreelManager<QueryProcessorType>::getShowreels(
    Handler<nx::vms::api::ShowreelDataList> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().template processQueryAsync<const nx::Uuid&, nx::vms::api::ShowreelDataList>(
        ApiCommand::getShowreels,
        nx::Uuid(),
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class QueryProcessorType>
int ShowreelManager<QueryProcessorType>::save(
    const nx::vms::api::ShowreelData& data,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::saveShowreel,
        data,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class QueryProcessorType>
int ShowreelManager<QueryProcessorType>::remove(
    const nx::Uuid& tourId,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::removeShowreel,
        nx::vms::api::IdData(tourId),
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

} // namespace ec2
