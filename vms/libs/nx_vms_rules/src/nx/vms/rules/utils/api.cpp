// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "api.h"

#include <nx/utils/qobject.h>
#include <nx/vms/api/rules/action_info.h>
#include <nx/vms/api/rules/event_info.h>
#include <nx/vms/api/rules/rule.h>
#include <nx/vms/common/utils/schedule.h>

#include "../action_builder.h"
#include "../action_builder_field.h"
#include "../basic_action.h"
#include "../basic_event.h"
#include "../event_filter.h"
#include "../event_filter_field.h"
#include "../rule.h"
#include "serialization.h"

namespace nx::vms::rules {

api::Rule serialize(const Rule* rule)
{
    NX_ASSERT(rule);

    api::Rule result;

    result.id = rule->id();

    for (auto filter: rule->eventFilters())
    {
        result.eventList += serialize(filter);
    }
    for (auto builder: rule->actionBuilders())
    {
        result.actionList += serialize(builder);
    }
    result.comment = rule->comment();
    result.enabled = rule->enabled();
    result.internal = rule->isInternal();
    result.schedule = nx::vms::common::scheduleFromByteArray(rule->schedule());

    return result;
}

api::EventFilter serialize(const EventFilter* filter)
{
    NX_ASSERT(filter);

    api::EventFilter result;

    const auto fields = filter->fields();
    for (const auto& [name, field]: filter->fields().asKeyValueRange())
    {
        if (!field->properties().visible)
            continue;

        api::Field serialized;
        serialized.type = field->metatype();
        serialized.props = field->serializedProperties();

        result.fields.emplace(name, std::move(serialized));
    }
    result.id = filter->id();
    result.type = filter->eventType();

    return result;
}

api::ActionBuilder serialize(const ActionBuilder* builder)
{
    NX_ASSERT(builder);

    api::ActionBuilder result;

    const auto fields = builder->fields();
    for (const auto& [name, field]: builder->fields().asKeyValueRange())
    {
        if (!field->properties().visible)
            continue;

        api::Field serialized;
        serialized.type = field->metatype();
        serialized.props = field->serializedProperties();

        result.fields.emplace(name, std::move(serialized));
    }
    result.id = builder->id();
    result.type = builder->actionType();

    return result;
}

api::ActionInfo serialize(const BasicAction* action, const QSet<QByteArray>& excludedProperties)
{
    auto props = nx::utils::propertyNames(action) - excludedProperties;

    return api::ActionInfo{
        .ruleId = action->ruleId(),
        .props = serializeProperties(action, props),
    };
}

api::EventInfo serialize(const BasicEvent* event, UuidList ruleIds)
{
    nx::vms::api::rules::EventInfo result;

    result.props = serializeProperties(event, nx::utils::propertyNames(event));

    for (auto id: ruleIds)
        result.triggeredRules.push_back(id);

    return result;
}

} // namespace nx::vms::rules
