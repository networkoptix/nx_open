#include "wearable_camera_resource.h"

const QString QnWearableCameraResource::kManufacture = lit("WEARABLE_CAMERA");

QnWearableCameraResource::QnWearableCameraResource()
{
    removeFlags(Qn::live | Qn::network | Qn::streamprovider);
    addFlags(Qn::wearable_camera);
}

QnWearableCameraResource::~QnWearableCameraResource()
{
}

QString QnWearableCameraResource::getDriverName() const
{
    return kManufacture;
}

QnAbstractStreamDataProvider* QnWearableCameraResource::createLiveDataProvider()
{
    return nullptr;
}

