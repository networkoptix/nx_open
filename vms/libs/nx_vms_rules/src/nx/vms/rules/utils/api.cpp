// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "api.h"

#include <nx/vms/api/rules/rule.h>
#include <nx/vms/common/utils/schedule.h>

#include "../action_builder.h"
#include "../action_builder_field.h"
#include "../event_filter.h"
#include "../event_filter_field.h"
#include "../rule.h"

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
    result.system = rule->isSystem();
    result.schedule = nx::vms::common::scheduleFromByteArray(rule->schedule());

    return result;
}

api::EventFilter serialize(const EventFilter* filter)
{
    NX_ASSERT(filter);

    api::EventFilter result;

    const auto fields = filter->fields();
    for (auto it = fields.cbegin(); it != fields.cend(); ++it)
    {
        const auto& name = it.key();
        const auto field = it.value();

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
    for (auto it = fields.cbegin(); it != fields.cend(); ++it)
    {
        const auto& name = it.key();
        const auto field = it.value();

        api::Field serialized;
        serialized.type = field->metatype();
        serialized.props = field->serializedProperties();

        result.fields.emplace(name, std::move(serialized));
    }
    result.id = builder->id();
    result.type = builder->actionType();

    return result;
}

} // namespace nx::vms::rules
