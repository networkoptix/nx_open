#pragma once

#include "id_data.h"

#include <vector>

#include <QtCore/QString>
#include <QtCore/QtGlobal>
#include <QtCore/QByteArray>

#include <nx/utils/latin1_array.h>
#include <nx/vms/api/types/event_rule_types.h>

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API EventRuleData: IdData
{
    EventType eventType = EventType::undefinedEvent;
    std::vector<QnUuid> eventResourceIds;
    QnLatin1Array eventCondition;
    EventState eventState = EventState::undefined;

    ActionType actionType = ActionType::undefinedAction;
    std::vector<QnUuid> actionResourceIds;
    QnLatin1Array actionParams;

    qint32 aggregationPeriod = 0; //< Seconds.
    bool disabled = false;
    QString comment;
    QString schedule;

    bool system = false; // system rule cannot be deleted
};

#define EventRuleData_Fields IdData_Fields \
    (eventType)(eventResourceIds)(eventCondition) \
    (eventState)(actionType)(actionResourceIds)(actionParams) \
    (aggregationPeriod)(disabled)(comment)(schedule)(system)

struct NX_VMS_API EventActionData: Data
{
    ActionType actionType = ActionType::undefinedAction;
    EventState toggleState = EventState::undefined;
    bool receivedFromRemoteHost = false;
    std::vector<QnUuid> resourceIds;
    QByteArray params;
    QByteArray runtimeParams;
    QnUuid ruleId;
    qint32 aggregationCount = 0;
};

#define EventActionData_Fields (actionType)(toggleState)(receivedFromRemoteHost)(resourceIds) \
    (params)(runtimeParams)(ruleId)(aggregationCount)

struct NX_VMS_API ResetEventRulesData: Data
{
    // TODO: #rvasilenko these rules are not used right now. Cannot remove as this type is used
    // to deduct transaction type by template substitution.
    EventRuleDataList defaultRules;
};

#define ResetEventRulesData_Fields (defaultRules)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::EventRuleData)
Q_DECLARE_METATYPE(nx::vms::api::EventRuleDataList)
Q_DECLARE_METATYPE(nx::vms::api::EventActionData)
Q_DECLARE_METATYPE(nx::vms::api::EventActionDataList)
