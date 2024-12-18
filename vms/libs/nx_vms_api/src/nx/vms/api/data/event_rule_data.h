// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <QtCore/QByteArray>
#include <QtCore/QString>

#include <nx/utils/latin1_array.h>
#include <nx/vms/api/types/event_rule_types.h>

#include "id_data.h"

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API EventRuleData: IdData
{
    EventType eventType = EventType::undefinedEvent;
    std::vector<nx::Uuid> eventResourceIds;

    /**%apidoc
     * JSON object serialized using the Latin-1 encoding, even though it may contain other Unicode
     * characters.
     */
    QnLatin1Array eventCondition;

    EventState eventState = EventState::undefined;
    ActionType actionType = ActionType::undefinedAction;
    std::vector<nx::Uuid> actionResourceIds;

    /**%apidoc
     * JSON object serialized using the Latin-1 encoding, even though it may contain other Unicode
     * characters.
     */
    QnLatin1Array actionParams;

    qint32 aggregationPeriod = 0; //< Seconds.
    bool disabled = false;
    QString comment;
    QString schedule;

    bool system = false; // system rule cannot be deleted

    bool operator==(const EventRuleData& other) const = default;
};
#define EventRuleData_Fields IdData_Fields \
    (eventType)(eventResourceIds)(eventCondition) \
    (eventState)(actionType)(actionResourceIds)(actionParams) \
    (aggregationPeriod)(disabled)(comment)(schedule)(system)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(EventRuleData)
NX_REFLECTION_INSTRUMENT(EventRuleData, EventRuleData_Fields);

struct NX_VMS_API EventActionData
{
    ActionType actionType = ActionType::undefinedAction;
    EventState toggleState = EventState::undefined;
    bool receivedFromRemoteHost = false;
    std::vector<nx::Uuid> resourceIds;
    QByteArray params;
    QByteArray runtimeParams;
    nx::Uuid ruleId;
    qint32 aggregationCount = 0;

    bool operator==(const EventActionData& other) const = default;
};
#define EventActionData_Fields (actionType)(toggleState)(receivedFromRemoteHost)(resourceIds) \
    (params)(runtimeParams)(ruleId)(aggregationCount)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(EventActionData)
NX_REFLECTION_INSTRUMENT(EventActionData, EventActionData_Fields);

struct NX_VMS_API ResetEventRulesData
{
    // TODO: #rvasilenko these rules are not used right now. Cannot remove as this type is used
    // to deduct transaction type by template substitution.
    EventRuleDataList defaultRules;
};
#define ResetEventRulesData_Fields (defaultRules)
NX_VMS_API_DECLARE_STRUCT(ResetEventRulesData)
NX_REFLECTION_INSTRUMENT(ResetEventRulesData, ResetEventRulesData_Fields);

} // namespace api
} // namespace vms
} // namespace nx
