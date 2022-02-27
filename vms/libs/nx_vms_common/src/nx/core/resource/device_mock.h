// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <core/resource/camera_resource.h>
#include <nx/core/resource/resource_with_local_property_storage.h>

namespace nx::core::resource {

class NX_VMS_COMMON_API DeviceMock: public ResourceWithLocalPropertyStorage<QnVirtualCameraResource>
{
    using base_type = ResourceWithLocalPropertyStorage<QnVirtualCameraResource>;

public:
    DeviceMock(QnCommonModule* commonModule = nullptr): base_type(commonModule) {};

    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;

    void setAnalyticsSdkEventMapping(
        std::map<nx::vms::api::EventType, QString> setAnalyticsSdkEventMapping);

    virtual QString vmsToAnalyticsEventTypeId(nx::vms::api::EventType eventType) const override;

private:
    std::map<nx::vms::api::EventType, QString> m_analyticsSdkEventMapping;
};

using DeviceMockPtr = QnSharedResourcePointer<DeviceMock>;

} // namespace nx::core::resource
