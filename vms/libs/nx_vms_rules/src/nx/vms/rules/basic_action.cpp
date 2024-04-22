// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "basic_action.h"

#include <QtCore/QMetaProperty>

#include <nx/utils/log/assert.h>

#include "utils/type.h"

namespace nx::vms::rules {

QString BasicAction::type() const
{
    return utils::type(metaObject()); //< Assert?
}

std::chrono::microseconds BasicAction::timestamp() const
{
    return m_timestamp;
}

void BasicAction::setTimestamp(std::chrono::microseconds timestamp)
{
    m_timestamp = timestamp;
}

State BasicAction::state() const
{
    return m_state;
}

void BasicAction::setState(State state)
{
    NX_ASSERT(state != State::none);
    m_state = state;
}

nx::Uuid BasicAction::ruleId() const
{
    return m_ruleId;
}

void BasicAction::setRuleId(const nx::Uuid& ruleId)
{
    m_ruleId = ruleId;
}

nx::Uuid BasicAction::originPeerId() const
{
    return m_originPeerId;
}

void BasicAction::setOriginPeerId(const nx::Uuid & peerId)
{
    NX_ASSERT(!peerId.isNull());
    m_originPeerId = peerId;
}

QString BasicAction::uniqueKey() const
{
    return type();
}

QVariantMap BasicAction::details(common::SystemContext* /*context*/) const
{
    return {};
}

} // namespace nx::vms::rules
