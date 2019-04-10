#pragma once

#include <nx_ec/ec_api_fwd.h>
#include <nx/vms/api/data/event_rule_data.h>
#include <nx_ec/impl/ec_api_impl.h>
#include <nx_ec/impl/sync_handler.h>

namespace ec2 {

class AbstractBusinessEventNotificationManager: public QObject
{
    Q_OBJECT

signals:
    void addedOrUpdated(const nx::vms::api::EventRuleData& rule, ec2::NotificationSource source);
    void removed(const QnUuid& id);
    void businessRuleReset(const nx::vms::api::EventRuleDataList& rules);
    void gotBroadcastAction(const nx::vms::event::AbstractActionPtr& action);
};

using AbstractBusinessEventNotificationManagerPtr =
std::shared_ptr<AbstractBusinessEventNotificationManager>;

/*!
    \note All methods are asynchronous if other not specified
*/
class AbstractEventRulesManager
{
public:
    virtual ~AbstractEventRulesManager() = default;

    /*!
        \param handler Functor with params: (ErrorCode, const nx::vms::api::EventRuleDataList&)
    */
    template<class TargetType, class HandlerType>
    int getEventRules(TargetType* target, HandlerType handler)
    {
        return getEventRules(
            std::static_pointer_cast<impl::GetEventRulesHandler>(
                std::make_shared<impl::CustomGetEventRulesHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    ErrorCode getEventRulesSync(nx::vms::api::EventRuleDataList* const eventRulesList)
    {
        return impl::doSyncCall<impl::GetEventRulesHandler>(
            [this](impl::GetEventRulesHandlerPtr handler)
            {
                this->getEventRules(handler);
            },
            eventRulesList);
    }

    /*!
        \param handler Functor with params: (ErrorCode)
    */
    template<class TargetType, class HandlerType>
    int save(const nx::vms::api::EventRuleData& rule, TargetType* target, HandlerType handler)
    {
        return save(
            rule,
            std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    /*!
        \param handler Functor with params: (ErrorCode)
    */
    template<class TargetType, class HandlerType>
    int deleteRule(QnUuid ruleId, TargetType* target, HandlerType handler)
    {
        return deleteRule(
            ruleId,
            std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    /*!
        \param handler Functor with params: (ErrorCode)
    */
    template<class TargetType, class HandlerType>
    int broadcastEventAction(
        const nx::vms::api::EventActionData& actionData,
        TargetType* target,
        HandlerType handler)
    {
        return broadcastEventAction(
            actionData,
            std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    /*!
        \param handler Functor with params: (ErrorCode)
    */
    template<class TargetType, class HandlerType>
    int resetBusinessRules(TargetType* target, HandlerType handler)
    {
        return resetBusinessRules(
            std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

private:
    virtual int getEventRules(impl::GetEventRulesHandlerPtr handler) = 0;
    virtual int save(
        const nx::vms::api::EventRuleData& rule,
        impl::SimpleHandlerPtr handler) = 0;
    virtual int deleteRule(QnUuid ruleId, impl::SimpleHandlerPtr handler) = 0;
    virtual int broadcastEventAction(
        const nx::vms::api::EventActionData& actionData,
        impl::SimpleHandlerPtr handler) = 0;
    virtual int resetBusinessRules(impl::SimpleHandlerPtr handler) = 0;
};

} // namespace ec2
