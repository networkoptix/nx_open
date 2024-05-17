// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "vms_rules_type_comparator.h"

#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/manifest.h>
#include <nx/vms/rules/strings.h>

namespace nx::vms::client::desktop {

using namespace nx::vms::rules;

VmsRulesTypeComparator::VmsRulesTypeComparator(SystemContext* context):
    SystemContextAware(context)
{
    initLexOrdering();
}

bool VmsRulesTypeComparator::lexEventLess(const QString& left, const QString& right) const
{
    return toLexEventType(left) < toLexEventType(right);
}

bool VmsRulesTypeComparator::lexActionLess(const QString& left, const QString& right) const
{
    return toLexActionType(left) < toLexActionType(right);
}

void VmsRulesTypeComparator::initLexOrdering()
{
    const auto engine = systemContext()->vmsRulesEngine();

    std::map<QString, QString> typeByDisplayName;
    for (const auto& descriptor: engine->events())
        typeByDisplayName.emplace(descriptor.id, descriptor.displayName);

    int i = 0;
    for (const auto& [name, type]: typeByDisplayName)
        m_eventTypeToLexOrder[type] = i++;

    typeByDisplayName.clear();
    i = 0;

    for (const auto& descriptor: engine->actions())
        typeByDisplayName.emplace(descriptor.id, descriptor.displayName);

    for (const auto& [name, type]: typeByDisplayName)
        m_actionTypeToLexOrder[type] = i++;
}

int VmsRulesTypeComparator::toLexEventType(const QString& eventType) const
{
    return m_eventTypeToLexOrder[eventType];
}

int VmsRulesTypeComparator::toLexActionType(const QString& actionType) const
{
    return m_actionTypeToLexOrder[actionType];
}

QStringList VmsRulesTypeComparator::lexSortedEvents(const QStringList& events) const
{
    auto result = events;
    std::sort(result.begin(), result.end(),
        [this](const QString& l, const QString& r)
        {
            return lexEventLess(l, r);
        });
    return result;
}

QStringList VmsRulesTypeComparator::lexSortedActions(const QStringList& actions) const
{
    auto result = actions;
    std::sort(result.begin(), result.end(),
        [this](const QString& l, const QString& r)
        {
            return lexActionLess(l, r);
        });
    return result;
}

} // namespace nx::vms::client::desktop
