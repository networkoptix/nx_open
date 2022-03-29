// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_engine_resource_mock.h"

namespace nx {

bool AnalyticsEngineResourceMock::isDeviceDependent() const
{
    return m_isDeviceDependent;
}

std::set<QString> AnalyticsEngineResourceMock::eventTypeIds() const
{
    return m_eventTypeIds;
}

std::set<QString> AnalyticsEngineResourceMock::objectTypeIds() const
{
    return m_objectTypeIds;
}

void AnalyticsEngineResourceMock::setIsDeviceDependent(bool value)
{
    m_isDeviceDependent = value;
}

void AnalyticsEngineResourceMock::setAnalyticsEventTypeIds(std::set<QString> eventTypeIds)
{
    m_eventTypeIds = std::move(eventTypeIds);
}

void AnalyticsEngineResourceMock::setAnalyticsObjectTypeIds(std::set<QString> objectTypeIds)
{
    m_objectTypeIds = std::move(objectTypeIds);
}

} // namespace nx
