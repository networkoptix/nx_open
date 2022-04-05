// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "basic_action.h"

#include <QtCore/QMetaProperty>

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

QString BasicAction::eventType() const
{
    return m_eventType;
}

void BasicAction::setEventType(const QString& eventType)
{
    m_eventType = eventType;
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
