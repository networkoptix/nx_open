// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/async_handler_executor.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/event_rule_data.h>
#include <nx/vms/event/event_fwd.h>

#include "../ec_api_common.h"

namespace ec2 {

class NX_VMS_COMMON_API AbstractBusinessEventNotificationManager: public QObject
{
    Q_OBJECT

signals:
    void addedOrUpdated(const nx::vms::api::EventRuleData& rule, ec2::NotificationSource source);
    void removed(const nx::Uuid& id);
    void businessRuleReset(const nx::vms::api::EventRuleDataList& rules);
    void gotBroadcastAction(const QSharedPointer<nx::vms::event::AbstractAction>& action);
};

/*!
    \note All methods are asynchronous if other not specified
*/
class NX_VMS_COMMON_API AbstractEventRulesManager
{
public:
    virtual ~AbstractEventRulesManager() = default;

    virtual int getEventRules(
        Handler<nx::vms::api::EventRuleDataList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode getEventRulesSync(nx::vms::api::EventRuleDataList* outDataList);

    virtual int save(
        const nx::vms::api::EventRuleData& data,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode saveSync(const nx::vms::api::EventRuleData& data);

    virtual int deleteRule(
        const nx::Uuid& ruleId,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode deleteRuleSync(const nx::Uuid& ruleId);

    virtual int broadcastEventAction(
        const nx::vms::api::EventActionData& data,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode broadcastEventActionSync(const nx::vms::api::EventActionData& data);

    virtual int resetBusinessRules(
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode resetBusinessRulesSync();
};

} // namespace ec2
