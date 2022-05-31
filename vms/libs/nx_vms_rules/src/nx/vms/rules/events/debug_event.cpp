// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "debug_event.h"

#include "../event_fields/int_field.h"
#include "../event_fields/text_field.h"
#include "../utils/event_details.h"

namespace nx::vms::rules {

DebugEvent::DebugEvent(const QString& action, qint64 value, std::chrono::microseconds timestamp):
    BasicEvent(timestamp),
    m_action(action),
    m_value(value)
{
}

QVariantMap DebugEvent::details(common::SystemContext* context) const
{
    auto result = BasicEvent::details(context);

    utils::insertIfNotEmpty(result, utils::kDescriptionDetailName, description());
    result.insert(utils::kEmailTemplatePathDetailName, manifest().emailTemplatePath);

    return result;
}

QString DebugEvent::description() const
{
    return QString("%1 action with %2 value").arg(m_action).arg(m_value);
}

const ItemDescriptor& DebugEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = "nx.events.debug",
        .displayName = tr("Debug Event"),
        .description = "",
        .fields = {
            makeFieldDescriptor<EventTextField>("action", tr("Action")),
            makeFieldDescriptor<IntField>("value", tr("Value"))
        },
        .emailTemplatePath = ":/email_templates/debug.mustache"
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
