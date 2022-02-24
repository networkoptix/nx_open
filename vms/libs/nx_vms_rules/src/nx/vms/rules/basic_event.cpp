// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "basic_event.h"

#include <QDateTime>
#include <QMetaProperty>

namespace nx::vms::rules {

BasicEvent::BasicEvent(nx::vms::api::rules::EventInfo& info)
    :
    m_type(info.eventType),
    m_state(info.state),
    m_timestamp(QDateTime::currentMSecsSinceEpoch()) //< qnSyncTime?
{
}

BasicEvent::BasicEvent()
    :
    m_state(State::none),
    m_timestamp(QDateTime::currentMSecsSinceEpoch()) //< qnSyncTime?
{
}

QString BasicEvent::type() const
{
    if (!m_type.isEmpty())
        return m_type;

    if (auto index = metaObject()->indexOfClassInfo("type"); index != -1)
        return metaObject()->classInfo(index).value();

    return {}; //< Assert?
}

QStringList BasicEvent::fields() const
{
    QStringList result;

    auto meta = metaObject();
    for (int i = base_type::staticMetaObject.propertyOffset(); i < meta->propertyCount(); ++i)
    {
        const auto& propName = meta->property(i).name();
        result << propName;
    }

    for (const auto& propName: this->dynamicPropertyNames())
    {
        result << propName;
    }

    return result;
}

} // namespace nx::vms::rules
