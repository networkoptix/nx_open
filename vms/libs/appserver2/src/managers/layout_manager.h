// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/rest/user_access_data.h>
#include <nx/vms/api/data/layout_data.h>
#include <nx_ec/managers/abstract_layout_manager.h>
#include <transaction/transaction.h>

namespace ec2 {

template<class QueryProcessorType>
class QnLayoutManager: public AbstractLayoutManager
{
public:
    QnLayoutManager(QueryProcessorType* queryProcessor, const nx::network::rest::audit::Record& auditRecord);

    virtual int getLayouts(
        Handler<nx::vms::api::LayoutDataList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int save(
        const nx::vms::api::LayoutData& data,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int remove(
        const nx::Uuid& layoutId,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

private:
    decltype(auto) processor() { return m_queryProcessor->getAccess(m_auditRecord); }

private:
    QueryProcessorType* const m_queryProcessor;
    nx::network::rest::audit::Record m_auditRecord;
};

template<typename QueryProcessorType>
QnLayoutManager<QueryProcessorType>::QnLayoutManager(
    QueryProcessorType* queryProcessor, const nx::network::rest::audit::Record& auditRecord)
    :
    m_queryProcessor(queryProcessor),
    m_auditRecord(auditRecord)
{
}

template<class QueryProcessorType>
int QnLayoutManager<QueryProcessorType>::getLayouts(
    Handler<nx::vms::api::LayoutDataList> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().template processQueryAsync<const nx::Uuid&, nx::vms::api::LayoutDataList>(
        ApiCommand::getLayouts,
        nx::Uuid(),
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class QueryProcessorType>
int QnLayoutManager<QueryProcessorType>::save(
    const nx::vms::api::LayoutData& data,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::saveLayout,
        data,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class QueryProcessorType>
int QnLayoutManager<QueryProcessorType>::remove(
    const nx::Uuid& layoutId,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::removeLayout,
        nx::vms::api::IdData(layoutId),
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

} // namespace ec2
