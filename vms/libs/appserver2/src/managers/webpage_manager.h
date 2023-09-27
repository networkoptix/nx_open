// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource_access/user_access_data.h>
#include <nx_ec/managers/abstract_webpage_manager.h>
#include <transaction/transaction.h>

namespace ec2 {

template<class QueryProcessorType>
class QnWebPageManager: public AbstractWebPageManager
{
public:
    QnWebPageManager(QueryProcessorType* queryProcessor, const Qn::UserSession& userSession);

    virtual int getWebPages(
        Handler<nx::vms::api::WebPageDataList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int save(
        const nx::vms::api::WebPageData& data,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int remove(
        const QnUuid& id,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

private:
    decltype(auto) processor() { return m_queryProcessor->getAccess(m_userSession); }

private:
    QueryProcessorType* const m_queryProcessor;
    Qn::UserSession m_userSession;
};

template<class QueryProcessorType>
QnWebPageManager<QueryProcessorType>::QnWebPageManager(
    QueryProcessorType* queryProcessor,
    const Qn::UserSession& userSession)
    :
    m_queryProcessor(queryProcessor),
    m_userSession(userSession)
{
}

template<class QueryProcessorType>
int QnWebPageManager<QueryProcessorType>::getWebPages(
    Handler<nx::vms::api::WebPageDataList> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().template processQueryAsync<QnUuid, nx::vms::api::WebPageDataList>(
        ApiCommand::getWebPages,
        QnUuid(),
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class QueryProcessorType>
int QnWebPageManager<QueryProcessorType>::save(
    const nx::vms::api::WebPageData& data,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::saveWebPage,
        data,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class QueryProcessorType>
int QnWebPageManager<QueryProcessorType>::remove(
    const QnUuid& id,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::removeWebPage,
        nx::vms::api::IdData(id),
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

} // namespace ec2
