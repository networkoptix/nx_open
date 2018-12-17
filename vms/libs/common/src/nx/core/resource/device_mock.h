#include <core/resource/camera_resource.h>

namespace nx::core::resource {

class DeviceMock: public QnVirtualCameraResource
{
public:
    virtual QString getDriverName() const override;

    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;

    virtual bool saveProperties() override;
};

} // namespace nx::core::resource
