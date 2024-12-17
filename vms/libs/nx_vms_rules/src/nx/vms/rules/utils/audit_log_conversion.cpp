// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "audit_log_conversion.h"

#include <QtCore/QCoreApplication>

#include <nx/vms/api/rules/rule.h>

#include "../action_builder.h"
#include "../event_filter.h"
#include "../rule.h"
#include "../strings.h"

namespace nx::vms::rules::utils {

namespace {

static const QString kEntitiesDelimiter(", ");
static const QString kRuleDelimiter("-->");

} // namespace

struct RulesAuditLogStrings
{
    Q_DECLARE_TR_FUNCTIONS(RulesAuditLogStrings)
};

QString toAuditLogFormat(const nx::vms::api::rules::Rule& rule)
{
    QStringList events;
    for (const auto& filter: rule.eventList)
        events.push_back(filter.type);
    QStringList actions;
    for (const auto& action: rule.actionList)
        actions.push_back(action.type);
    return nx::format("%1 --> %2",
        events.join(kEntitiesDelimiter),
        actions.join(kEntitiesDelimiter)).toQString();
}

QString toAuditLogFormat(const ConstRulePtr& rule)
{
    QStringList events;
    for (const auto& filter: rule->eventFilters())
        events.push_back(filter->eventType());
    QStringList actions;
    for (const auto& action: rule->actionBuilders())
        actions.push_back(action->actionType());
    return nx::format("%1 --> %2",
        events.join(kEntitiesDelimiter),
        actions.join(kEntitiesDelimiter)).toQString();
}

QString detailedTextFromAuditLogFormat(const QString& encoded, Engine* engine)
{
    QStringList eventsAndActions = encoded.split(kRuleDelimiter);
    if (eventsAndActions.size() != 2)
        return encoded; //< Old or incorrect format.

    QStringList eventIds = eventsAndActions[0].split(kEntitiesDelimiter);
    QStringList actionIds = eventsAndActions[1].split(kEntitiesDelimiter);
    if (eventIds.empty() || actionIds.empty())
        return encoded; //< Old or incorrect format.

    QStringList events;
    for (const auto& id: eventIds)
        events.push_back(Strings::eventName(engine, id.trimmed()));

    QStringList actions;
    for (const auto& id: actionIds)
        actions.push_back(Strings::actionName(engine, id.trimmed()));

    return RulesAuditLogStrings::tr("On %1 --> %2", "%1 is the event name, %2 is the action name")
        .arg(events.join(kEntitiesDelimiter), actions.join(kEntitiesDelimiter));
}

} // namespace nx::vms::rules::utils
