// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource_access/user_access_data.h>
#include <nx_ec/managers/abstract_analytics_manager.h>
#include <transaction/transaction.h>

namespace ec2 {

template<typename QueryProcessor>
class AnalyticsManager: public AbstractAnalyticsManager
{
public:
    AnalyticsManager(QueryProcessor* queryProcessor, const Qn::UserSession& userSession);

    virtual int getAnalyticsPlugins(
        Handler<nx::vms::api::AnalyticsPluginDataList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int getAnalyticsEngines(
        Handler<nx::vms::api::AnalyticsEngineDataList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int save(
        const nx::vms::api::AnalyticsPluginData& analyticsPlugin,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int save(
        const nx::vms::api::AnalyticsEngineData& analyticsEngine,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int removeAnalyticsPlugin(
        const QnUuid& id,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int removeAnalyticsEngine(
        const QnUuid& id,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

private:
    decltype(auto) processor() { return m_queryProcessor->getAccess(m_userSession); }

private:
    QueryProcessor* m_queryProcessor;
    Qn::UserSession m_userSession;
};

template<typename QueryProcessor>
AnalyticsManager<QueryProcessor>::AnalyticsManager(
    QueryProcessor* queryProcessor, const Qn::UserSession& userSession)
    :
    m_queryProcessor(queryProcessor),
    m_userSession(userSession)
{
}

template<typename QueryProcessor>
int AnalyticsManager<QueryProcessor>::getAnalyticsPlugins(
    Handler<nx::vms::api::AnalyticsPluginDataList> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().template processQueryAsync<QnUuid, nx::vms::api::AnalyticsPluginDataList>(
        ApiCommand::getAnalyticsPlugins,
        QnUuid(),
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<typename QueryProcessor>
int AnalyticsManager<QueryProcessor>::getAnalyticsEngines(
    Handler<nx::vms::api::AnalyticsEngineDataList> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().template processQueryAsync<QnUuid, nx::vms::api::AnalyticsEngineDataList>(
        ApiCommand::getAnalyticsEngines,
        QnUuid(),
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<typename QueryProcessor>
int AnalyticsManager<QueryProcessor>::save(
    const nx::vms::api::AnalyticsPluginData& data,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::saveAnalyticsPlugin,
        data,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<typename QueryProcessor>
int AnalyticsManager<QueryProcessor>::save(
    const nx::vms::api::AnalyticsEngineData& data,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::saveAnalyticsEngine,
        data,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<typename QueryProcessor>
int AnalyticsManager<QueryProcessor>::removeAnalyticsPlugin(
    const QnUuid& id, Handler<> handler, nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::removeAnalyticsPlugin,
        nx::vms::api::IdData(id),
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<typename QueryProcessor>
int AnalyticsManager<QueryProcessor>::removeAnalyticsEngine(
    const QnUuid& id, Handler<> handler, nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::removeAnalyticsEngine,
        nx::vms::api::IdData(id),
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

} // namespace ec2
