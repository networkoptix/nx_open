// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/concurrent.h>
#include <nx/vms/ec2/ec2_thread_pool.h>
#include <nx_ec/managers/abstract_event_rules_manager.h>
#include <transaction/transaction.h>

namespace ec2 {

template<class QueryProcessorType>
class EventRulesManager: public AbstractEventRulesManager
{
public:
    EventRulesManager(QueryProcessorType* queryProcessor, const Qn::UserSession& userSession);

    virtual int getEventRules(
        Handler<nx::vms::api::EventRuleDataList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int save(
        const nx::vms::api::EventRuleData& data,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int deleteRule(
        const QnUuid& ruleId,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int broadcastEventAction(
        const nx::vms::api::EventActionData& data,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int resetBusinessRules(
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

private:
    decltype(auto) processor() { return m_queryProcessor->getAccess(m_userSession); }

private:
    QueryProcessorType* const m_queryProcessor;
    Qn::UserSession m_userSession;
};

template<class QueryProcessorType>
EventRulesManager<QueryProcessorType>::EventRulesManager(
    QueryProcessorType* queryProcessor, const Qn::UserSession& userSession)
    :
    m_queryProcessor(queryProcessor),
    m_userSession(userSession)
{
}

template<class T>
int EventRulesManager<T>::getEventRules(
    Handler<nx::vms::api::EventRuleDataList> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().template processQueryAsync<QnUuid, nx::vms::api::EventRuleDataList>(
        ApiCommand::getEventRules,
        QnUuid(),
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class T>
int EventRulesManager<T>::save(
    const nx::vms::api::EventRuleData& data,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::saveEventRule,
        data,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class T>
int EventRulesManager<T>::deleteRule(
    const QnUuid& ruleId,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::removeEventRule,
        nx::vms::api::IdData(ruleId),
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class T>
int EventRulesManager<T>::broadcastEventAction(
    const nx::vms::api::EventActionData& data,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    using namespace std::placeholders;
    processor().processUpdateAsync(
        ApiCommand::broadcastAction,
        data,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class T>
int EventRulesManager<T>::resetBusinessRules(
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    // providing event rules set from the client side is rather incorrect way of reset.
    processor().processUpdateAsync(
        ApiCommand::resetEventRules,
        nx::vms::api::ResetEventRulesData{},
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

} // namespace ec2
