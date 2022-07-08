// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "basic_event.h"

#include <QtCore/QMetaProperty>

#include <nx/vms/time/formatter.h>
#include <nx/utils/metatypes.h>

#include "engine.h"
#include "utils/event_details.h"
#include "utils/type.h"

namespace nx::vms::rules {

BasicEvent::BasicEvent(const nx::vms::api::rules::EventInfo& info):
    m_type(info.eventType),
    m_state(info.state)
{
}

BasicEvent::BasicEvent(std::chrono::microseconds timestamp, State state):
    m_timestamp(timestamp),
    m_state(state)
{
}

QString BasicEvent::type() const
{
    if (!m_type.isEmpty())
        return m_type;

    return utils::type(metaObject()); //< Assert?
}

std::chrono::microseconds BasicEvent::timestamp() const
{
    return m_timestamp;
}

void BasicEvent::setTimestamp(const std::chrono::microseconds& timestamp)
{
    m_timestamp = timestamp;
}

State BasicEvent::state() const
{
    return m_state;
}

void BasicEvent::setState(State state)
{
    NX_ASSERT(state != State::none);
    m_state = state;
}

QString BasicEvent::uniqueName() const
{
    return type();
}

QString BasicEvent::resourceKey() const
{
    return {};
}

QVariantMap BasicEvent::details(common::SystemContext*) const
{
    QVariantMap result;

    utils::insertIfNotEmpty(result, utils::kTypeDetailName, type());
    utils::insertIfNotEmpty(result, utils::kNameDetailName, name());
    utils::insertIfNotEmpty(result, utils::kExtendedCaptionDetailName, extendedCaption());

    return result;
}

QString BasicEvent::name() const
{
    const auto descriptor = Engine::instance()->eventDescriptor(type());
    return descriptor ? descriptor->displayName : tr("Unknown event");
}

QString BasicEvent::extendedCaption() const
{
    return tr("%1 event has occurred").arg(name());
}

} // namespace nx::vms::rules
