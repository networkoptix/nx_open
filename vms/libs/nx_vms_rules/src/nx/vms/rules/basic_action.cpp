// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "basic_action.h"

#include <QtCore/QMetaProperty>

#include <nx/utils/log/assert.h>

#include "utils/type.h"

namespace nx::vms::rules {

QString BasicAction::type() const
{
    //if (!m_type.isEmpty())
    //    return m_type;

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

QnUuid BasicAction::ruleId() const
{
    return m_ruleId;
}

void BasicAction::setRuleId(const QnUuid& ruleId)
{
    m_ruleId = ruleId;
}

} // namespace nx::vms::rules
