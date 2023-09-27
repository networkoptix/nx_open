// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource_access/user_access_data.h>
#include <nx/vms/api/data/layout_data.h>
#include <nx_ec/managers/abstract_layout_manager.h>
#include <transaction/transaction.h>

namespace ec2 {

template<class QueryProcessorType>
class QnLayoutManager: public AbstractLayoutManager
{
public:
    QnLayoutManager(QueryProcessorType* queryProcessor, const Qn::UserSession& userSession);

    virtual int getLayouts(
        Handler<nx::vms::api::LayoutDataList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int save(
        const nx::vms::api::LayoutData& data,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int remove(
        const QnUuid& layoutId,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

private:
    decltype(auto) processor() { return m_queryProcessor->getAccess(m_userSession); }

private:
    QueryProcessorType* const m_queryProcessor;
    Qn::UserSession m_userSession;
};

template<typename QueryProcessorType>
QnLayoutManager<QueryProcessorType>::QnLayoutManager(
    QueryProcessorType* queryProcessor, const Qn::UserSession& userSession)
    :
    m_queryProcessor(queryProcessor),
    m_userSession(userSession)
{
}

template<class QueryProcessorType>
int QnLayoutManager<QueryProcessorType>::getLayouts(
    Handler<nx::vms::api::LayoutDataList> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().template processQueryAsync<const QnUuid&, nx::vms::api::LayoutDataList>(
        ApiCommand::getLayouts,
        QnUuid(),
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
    const QnUuid& layoutId,
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
