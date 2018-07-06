#pragma once

#include "nx_ec/ec_api.h"

#include <nx/utils/concurrent.h>
#include <ec2_thread_pool.h>
#include <transaction/transaction.h>
#include <nx/vms/event/rule.h>

using namespace nx;

namespace ec2 {

template<class QueryProcessorType>
class QnBusinessEventManager: public AbstractBusinessEventManager
{
public:
    QnBusinessEventManager(
        TransactionMessageBusAdapter* messageBus,
        QueryProcessorType* const queryProcessor,
        const Qn::UserAccessData& userAccessData);

    virtual int getBusinessRules(impl::GetBusinessRulesHandlerPtr handler) override;

    virtual int save(
        const nx::vms::event::RulePtr& rule,
        impl::SaveBusinessRuleHandlerPtr handler) override;
    virtual int deleteRule(QnUuid ruleId, impl::SimpleHandlerPtr handler) override;
    virtual int broadcastBusinessAction(
        const nx::vms::event::AbstractActionPtr& businessAction,
        impl::SimpleHandlerPtr handler) override;
    virtual int sendBusinessAction(
        const nx::vms::event::AbstractActionPtr& businessAction,
        const QnUuid& dstPeer,
        impl::SimpleHandlerPtr handler) override;
    virtual int resetBusinessRules(impl::SimpleHandlerPtr handler) override;

private:
    TransactionMessageBusAdapter* m_messageBus;
    QueryProcessorType* const m_queryProcessor;
    Qn::UserAccessData m_userAccessData;

    QnTransaction<nx::vms::api::EventActionData> prepareTransaction(
        ApiCommand::Value command,
        const nx::vms::event::AbstractActionPtr& resource);
};

template<class QueryProcessorType>
QnBusinessEventManager<QueryProcessorType>::QnBusinessEventManager(
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
int QnBusinessEventManager<T>::getBusinessRules(impl::GetBusinessRulesHandlerPtr handler)
{
    const int reqID = generateRequestID();

    auto queryDoneHandler = [reqID, handler, this](
        ErrorCode errorCode,
        const nx::vms::api::EventRuleDataList& rules)
        {
            vms::event::RuleList outData;
            if (errorCode == ErrorCode::ok)
                fromApiToResourceList(rules, outData);
            handler->done(reqID, errorCode, outData);
        };
    m_queryProcessor->getAccess(m_userAccessData).template processQueryAsync<
        QnUuid,
        nx::vms::api::EventRuleDataList,
        decltype(queryDoneHandler)>(ApiCommand::getEventRules, QnUuid(), queryDoneHandler);
    return reqID;
}

template<class T>
int QnBusinessEventManager<T>::save(
    const vms::event::RulePtr& rule,
    impl::SaveBusinessRuleHandlerPtr handler)
{
    const int reqID = generateRequestID();
    if (rule->id().isNull())
        rule->setId(QnUuid::createUuid());

    nx::vms::api::EventRuleData params;
    fromResourceToApi(rule, params);

    using namespace std::placeholders;
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
        ApiCommand::saveEventRule,
        params,
        std::bind(&impl::SaveBusinessRuleHandler::done, handler, reqID, _1, rule));

    return reqID;
}

template<class T>
int QnBusinessEventManager<T>::deleteRule(QnUuid ruleId, impl::SimpleHandlerPtr handler)
{
    const int reqID = generateRequestID();
    using namespace std::placeholders;
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
        ApiCommand::removeEventRule,
        nx::vms::api::IdData(ruleId),
        std::bind(std::mem_fn(&impl::SimpleHandler::done), handler, reqID, _1));
    return reqID;
}

template<class T>
int QnBusinessEventManager<T>::broadcastBusinessAction(
    const vms::event::AbstractActionPtr& businessAction,
    impl::SimpleHandlerPtr handler)
{
    const int reqID = generateRequestID();
    auto tran = prepareTransaction(ApiCommand::broadcastAction, businessAction);
    m_messageBus->sendTransaction(tran);
    nx::utils::concurrent::run(
        Ec2ThreadPool::instance(),
        std::bind(&impl::SimpleHandler::done, handler, reqID, ErrorCode::ok));
    return reqID;
}

template<class T>
int QnBusinessEventManager<T>::sendBusinessAction(
    const vms::event::AbstractActionPtr& businessAction,
    const QnUuid& dstPeer,
    impl::SimpleHandlerPtr handler)
{
    const int reqID = generateRequestID();
    auto tran = prepareTransaction(ApiCommand::execAction, businessAction);
    m_messageBus->sendTransaction(tran, dstPeer);
    nx::utils::concurrent::run(
        Ec2ThreadPool::instance(),
        std::bind(&impl::SimpleHandler::done, handler, reqID, ErrorCode::ok));
    return reqID;
}

template<class T>
int QnBusinessEventManager<T>::resetBusinessRules(impl::SimpleHandlerPtr handler)
{
    const int reqID = generateRequestID();
    nx::vms::api::ResetEventRulesData params;
    // providing event rules set from the client side is rather incorrect way of reset.

    using namespace std::placeholders;
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
        ApiCommand::resetEventRules,
        params,
        std::bind(std::mem_fn(&impl::SimpleHandler::done), handler, reqID, _1));

    return reqID;
}

template<class T>
QnTransaction<nx::vms::api::EventActionData> QnBusinessEventManager<T>::prepareTransaction(
    ApiCommand::Value command,
    const vms::event::AbstractActionPtr& resource)
{
    QnTransaction<nx::vms::api::EventActionData> tran(
        command,
        m_messageBus->commonModule()->moduleGUID());
    fromResourceToApi(resource, tran.params);
    return tran;
}

} // namespace ec2
