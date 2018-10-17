#pragma once

#include <nx_ec/ec_api.h>
#include <nx_ec/managers/abstract_event_rules_manager.h>

#include <nx/utils/concurrent.h>

#include <ec2_thread_pool.h>

#include <transaction/transaction.h>

namespace ec2 {

template<class QueryProcessorType>
class EventRulesManager: public AbstractEventRulesManager
{
public:
    EventRulesManager(
        TransactionMessageBusAdapter* messageBus,
        QueryProcessorType* const queryProcessor,
        const Qn::UserAccessData& userAccessData);

    virtual int getEventRules(impl::GetEventRulesHandlerPtr handler) override;

    virtual int save(
        const nx::vms::api::EventRuleData& rule,
        impl::SimpleHandlerPtr handler) override;
    virtual int deleteRule(QnUuid ruleId, impl::SimpleHandlerPtr handler) override;
    virtual int broadcastEventAction(
        const nx::vms::api::EventActionData& actionData,
        impl::SimpleHandlerPtr handler) override;
    virtual int sendEventAction(
        const nx::vms::api::EventActionData& actionData,
        const QnUuid& dstPeer,
        impl::SimpleHandlerPtr handler) override;
    virtual int resetBusinessRules(impl::SimpleHandlerPtr handler) override;

private:
    TransactionMessageBusAdapter* m_messageBus;
    QueryProcessorType* const m_queryProcessor;
    Qn::UserAccessData m_userAccessData;
};

template<class QueryProcessorType>
EventRulesManager<QueryProcessorType>::EventRulesManager(
    TransactionMessageBusAdapter* messageBus,
    QueryProcessorType* const queryProcessor,
    const Qn::UserAccessData& userAccessData)
    :
    m_messageBus(messageBus),
    m_queryProcessor(queryProcessor),
    m_userAccessData(userAccessData)
{
}

template<class T>
int EventRulesManager<T>::getEventRules(impl::GetEventRulesHandlerPtr handler)
{
    const int reqID = generateRequestID();

    auto queryDoneHandler = [reqID, handler, this](
        ErrorCode errorCode,
        const nx::vms::api::EventRuleDataList& rules)
        {
            handler->done(reqID, errorCode, rules);
        };
    m_queryProcessor->getAccess(m_userAccessData).template processQueryAsync<
        QnUuid,
        nx::vms::api::EventRuleDataList,
        decltype(queryDoneHandler)>(ApiCommand::getEventRules, QnUuid(), queryDoneHandler);
    return reqID;
}

template<class T>
int EventRulesManager<T>::save(
    const nx::vms::api::EventRuleData& rule,
    impl::SimpleHandlerPtr handler)
{
    const int reqID = generateRequestID();
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
        ApiCommand::saveEventRule,
        rule,
        [handler, reqID](ec2::ErrorCode errorCode){ handler->done(reqID, errorCode); });

    return reqID;
}

template<class T>
int EventRulesManager<T>::deleteRule(QnUuid ruleId, impl::SimpleHandlerPtr handler)
{
    const int reqID = generateRequestID();
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
        ApiCommand::removeEventRule,
        nx::vms::api::IdData(ruleId),
        [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
    return reqID;
}

template<class T>
int EventRulesManager<T>::broadcastEventAction(
    const nx::vms::api::EventActionData& actionData,
    impl::SimpleHandlerPtr handler)
{
    const int reqID = generateRequestID();
    using namespace std::placeholders;
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
        ApiCommand::broadcastAction,
        actionData,
        [handler, reqID](ec2::ErrorCode errorCode)
    {
        handler->done(reqID, errorCode);
    });
    return reqID;
}

template<class T>
int EventRulesManager<T>::sendEventAction(
    const nx::vms::api::EventActionData& actionData,
    const QnUuid& dstPeer,
    impl::SimpleHandlerPtr handler)
{
    const int reqID = generateRequestID();
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
        ApiCommand::execAction,
        actionData,
        [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
    return reqID;
}

template<class T>
int EventRulesManager<T>::resetBusinessRules(impl::SimpleHandlerPtr handler)
{
    const int reqID = generateRequestID();
    nx::vms::api::ResetEventRulesData params;
    // providing event rules set from the client side is rather incorrect way of reset.

    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
        ApiCommand::resetEventRules,
        params,
        [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });

    return reqID;
}

} // namespace ec2
