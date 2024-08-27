// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/async_handler_executor.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/rules/action_info.h>
#include <nx/vms/api/rules/event_info.h>
#include <nx/vms/api/rules/rule.h>

#include "../ec_api_common.h"

namespace ec2 {

class NX_VMS_COMMON_API AbstractVmsRulesNotificationManager: public QObject
{
    Q_OBJECT

signals:
    void ruleUpdated(const nx::vms::api::rules::Rule& rule, ec2::NotificationSource source);
    void ruleRemoved(const nx::Uuid& id);
    void reset();
    void eventReceived(const nx::vms::api::rules::EventInfo& eventInfo);
    void actionReceived(const nx::vms::api::rules::ActionInfo& actionInfo);
};

/*!
    \note All methods are asynchronous if other not specified
*/
class NX_VMS_COMMON_API AbstractVmsRulesManager
{
public:
    virtual ~AbstractVmsRulesManager() = default;

    virtual int getVmsRules(
        Handler<nx::vms::api::rules::RuleList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode getVmsRulesSync(nx::vms::api::rules::RuleList* outRuleList);

    virtual int save(
        const nx::vms::api::rules::Rule& rule,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode saveSync(const nx::vms::api::rules::Rule& rule);

    virtual int deleteRule(
        const nx::Uuid& id,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode deleteRuleSync(const nx::Uuid& id);

    virtual int transmitEvent(
        const nx::vms::api::rules::EventInfo& info,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    Result transmitEventSync(const nx::vms::api::rules::EventInfo& info);

    virtual int transmitAction(
        const nx::vms::api::rules::ActionInfo& info,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    Result transmitActionSync(const nx::vms::api::rules::ActionInfo& info);

    virtual int resetVmsRules(
        bool useDefault,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode resetVmsRulesSync(bool useDefault);
};

} // namespace ec2
