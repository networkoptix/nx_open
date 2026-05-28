// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/common/resource/analytics_engine_resource.h>

namespace nx {

class NX_VMS_COMMON_TEST_SUPPORT_API AnalyticsEngineResourceMock:
    public nx::vms::common::AnalyticsEngineResource
{
public:
    virtual bool isDeviceDependent() const override;

    virtual std::set<std::string> eventTypeIds() const override;

    virtual std::set<std::string> objectTypeIds() const override;

    void setIsDeviceDependent(bool value);

    void setAnalyticsEventTypeIds(std::set<std::string> eventTypeIds);

    void setAnalyticsObjectTypeIds(std::set<std::string> objectTypeIds);

private:
    bool m_isDeviceDependent = false;
    std::set<std::string> m_eventTypeIds;
    std::set<std::string> m_objectTypeIds;
};

using AnalyticsEngineResourceMockPtr = QnSharedResourcePointer<AnalyticsEngineResourceMock>;

} // namespace nx
