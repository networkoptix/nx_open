#include <core/resource/camera_resource.h>
#include <nx/core/resource/resource_with_local_property_storage.h>

namespace nx::core::resource {

class DeviceMock: public ResourceWithLocalPropertyStorage<QnVirtualCameraResource>
{
    using base_type = ResourceWithLocalPropertyStorage<QnVirtualCameraResource>;

public:
    DeviceMock(QnCommonModule* commonModule = nullptr): base_type(commonModule) {};

    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;

    virtual bool saveProperties() override;
};

} // namespace nx::core::resource
