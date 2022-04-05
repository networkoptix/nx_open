// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "debug_event.h"

#include "../event_fields/int_field.h"
#include "../event_fields/text_field.h"

namespace nx::vms::rules {

DebugEvent::DebugEvent(
    const QString& caption,
    const QString& description,
    const QString& action,
    qint64 value,
    std::chrono::microseconds timestamp)
    :
    BasicEvent(timestamp),
    m_action(action),
    m_value(value),
    m_caption(caption),
    m_description(description)
{
}

QString DebugEvent::caption() const
{
    return m_caption.isEmpty()
        ? manifest().displayName
        : m_caption;
}

void DebugEvent::setCaption(const QString& caption)
{
    m_caption = caption;
}

QString DebugEvent::description() const
{
    return m_description.isEmpty()
        ? QString("%1 action with value: %2").arg(m_action).arg(m_value)
        : m_description;
}

void DebugEvent::setDescription(const QString& description)
{
    m_description = description;
}

FilterManifest DebugEvent::filterManifest()
{
    return {};
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
        }
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
