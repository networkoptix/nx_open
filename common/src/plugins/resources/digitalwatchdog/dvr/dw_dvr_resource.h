#include "core/resource/camera_resource.h"

class QnDwDvrResource : public QnPhysicalCameraResource
{
public:
    static const char* MANUFACTURE;

    virtual QString manufacture() const override;
    virtual void setMotionMaskPhysical(int channel) override;
};
