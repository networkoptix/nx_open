// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_engine_resource_mock.h"

namespace nx {

bool AnalyticsEngineResourceMock::isDeviceDependent() const
{
    return m_isDeviceDependent;
}

std::set<std::string> AnalyticsEngineResourceMock::eventTypeIds() const
{
    return m_eventTypeIds;
}

std::set<std::string> AnalyticsEngineResourceMock::objectTypeIds() const
{
    return m_objectTypeIds;
}

void AnalyticsEngineResourceMock::setIsDeviceDependent(bool value)
{
    m_isDeviceDependent = value;
}

void AnalyticsEngineResourceMock::setAnalyticsEventTypeIds(std::set<std::string> eventTypeIds)
{
    m_eventTypeIds = std::move(eventTypeIds);
}

void AnalyticsEngineResourceMock::setAnalyticsObjectTypeIds(std::set<std::string> objectTypeIds)
{
    m_objectTypeIds = std::move(objectTypeIds);
}

} // namespace nx
