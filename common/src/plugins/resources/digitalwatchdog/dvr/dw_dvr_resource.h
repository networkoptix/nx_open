#include "core/resource/camera_resource.h"

class QnDwDvrResource : public QnPhysicalCameraResource
{
public:
    static const QString MANUFACTURE;

    virtual QString getDriverName() const override;
    virtual void setMotionMaskPhysical(int channel) override;
};
