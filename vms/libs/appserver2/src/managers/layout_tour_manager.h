// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <transaction/transaction.h>

#include <nx/vms/api/data/layout_tour_data.h>
#include <nx_ec/managers/abstract_layout_tour_manager.h>
#include <core/resource_access/user_access_data.h>

namespace ec2 {

template<class QueryProcessorType>
class QnLayoutTourManager: public AbstractLayoutTourManager
{
public:
    QnLayoutTourManager(QueryProcessorType* queryProcessor, const Qn::UserSession& userSession);

    virtual int getLayoutTours(
        Handler<nx::vms::api::LayoutTourDataList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int save(
        const nx::vms::api::LayoutTourData& data,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int remove(
        const QnUuid& tourId,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

private:
    decltype(auto) processor() { return m_queryProcessor->getAccess(m_userSession); }

private:
    QueryProcessorType* const m_queryProcessor;
    Qn::UserSession m_userSession;
};

template<typename QueryProcessorType>
QnLayoutTourManager<QueryProcessorType>::QnLayoutTourManager(
    QueryProcessorType* queryProcessor, const Qn::UserSession& userSession)
    :
    m_queryProcessor(queryProcessor),
    m_userSession(userSession)
{
}

template<class QueryProcessorType>
int QnLayoutTourManager<QueryProcessorType>::getLayoutTours(
    Handler<nx::vms::api::LayoutTourDataList> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    handler = handlerExecutor.bind(std::move(handler));
    const int requestId = generateRequestID();
    processor().template processQueryAsync<const QnUuid&, nx::vms::api::LayoutTourDataList>(
        ApiCommand::getLayoutTours,
        QnUuid(),
        [requestId, handler](auto&&... args) { handler(requestId, std::move(args)...); });
    return requestId;
}

template<class QueryProcessorType>
int QnLayoutTourManager<QueryProcessorType>::save(
    const nx::vms::api::LayoutTourData& data,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    handler = handlerExecutor.bind(std::move(handler));
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::saveLayoutTour,
        data,
        [requestId, handler](auto&&... args) { handler(requestId, std::move(args)...); });
    return requestId;
}

template<class QueryProcessorType>
int QnLayoutTourManager<QueryProcessorType>::remove(
    const QnUuid& tourId,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    handler = handlerExecutor.bind(std::move(handler));
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::removeLayoutTour,
        nx::vms::api::IdData(tourId),
        [requestId, handler](auto&&... args) { handler(requestId, std::move(args)...); });
    return requestId;
}

} // namespace ec2
