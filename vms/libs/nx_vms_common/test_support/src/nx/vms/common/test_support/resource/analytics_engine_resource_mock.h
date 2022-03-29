// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/common/resource/analytics_engine_resource.h>

namespace nx {

class NX_VMS_COMMON_TEST_SUPPORT_API AnalyticsEngineResourceMock:
    public nx::vms::common::AnalyticsEngineResource
{
public:
    virtual bool isDeviceDependent() const override;

    virtual std::set<QString> eventTypeIds() const override;

    virtual std::set<QString> objectTypeIds() const override;

    void setIsDeviceDependent(bool value);

    void setAnalyticsEventTypeIds(std::set<QString> eventTypeIds);

    void setAnalyticsObjectTypeIds(std::set<QString> objectTypeIds);

private:
    bool m_isDeviceDependent = false;
    std::set<QString> m_eventTypeIds;
    std::set<QString> m_objectTypeIds;
};

using AnalyticsEngineResourceMockPtr = QnSharedResourcePointer<AnalyticsEngineResourceMock>;

} // namespace nx
