// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/rest/user_access_data.h>
#include <nx/utils/concurrent.h>
#include <nx/vms/ec2/ec2_thread_pool.h>
#include <nx_ec/managers/abstract_vms_rules_manager.h>
#include <transaction/transaction.h>

namespace ec2 {

template<class QueryProcessorType>
class VmsRulesManager: public AbstractVmsRulesManager
{
public:
    VmsRulesManager(
        QueryProcessorType* queryProcessor, const nx::network::rest::audit::Record& auditRecord);

    virtual int getVmsRules(
        Handler<nx::vms::api::rules::RuleList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int save(
        const nx::vms::api::rules::Rule& rule,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int deleteRule(
        const nx::Uuid& id,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int transmitEvent(
        const nx::vms::api::rules::EventInfo& info,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int transmitAction(
        const nx::vms::api::rules::ActionInfo& info,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int resetVmsRules(
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

private:
    decltype(auto) processor() { return m_queryProcessor->getAccess(m_auditRecord); }

private:
    QueryProcessorType* const m_queryProcessor;
    nx::network::rest::audit::Record m_auditRecord;
};

template<class QueryProcessorType>
VmsRulesManager<QueryProcessorType>::VmsRulesManager(
    QueryProcessorType* queryProcessor, const nx::network::rest::audit::Record& auditRecord)
    :
    m_queryProcessor(queryProcessor),
    m_auditRecord(auditRecord)
{
}

template<class T>
int VmsRulesManager<T>::getVmsRules(
    Handler<nx::vms::api::rules::RuleList> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().template processQueryAsync<nx::Uuid, nx::vms::api::rules::RuleList>(
        ApiCommand::getVmsRules,
        nx::Uuid(),
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class T>
int VmsRulesManager<T>::save(
    const nx::vms::api::rules::Rule& rule,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::saveVmsRule,
        rule,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class T>
int VmsRulesManager<T>::deleteRule(
    const nx::Uuid& id,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::removeVmsRule,
        nx::vms::api::IdData(id),
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class T>
int VmsRulesManager<T>::transmitEvent(
    const nx::vms::api::rules::EventInfo& info,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::transmitVmsEvent,
        info,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class T>
int VmsRulesManager<T>::transmitAction(
    const nx::vms::api::rules::ActionInfo& info,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::transmitVmsAction,
        info,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class T>
int VmsRulesManager<T>::resetVmsRules(
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::resetVmsRules,
        nx::vms::api::rules::ResetRules{},
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

} // namespace ec2
