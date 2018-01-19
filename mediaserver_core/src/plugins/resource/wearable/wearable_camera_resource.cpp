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

void QnWearableCameraResource::setStatus(Qn::ResourceStatus, Qn::StatusChangeReason reason)
{
    // TODO: #wearable Maybe make it always offline instead?
    base_type::setStatus(Qn::Online, reason);
}

QnAbstractStreamDataProvider* QnWearableCameraResource::createLiveDataProvider()
{
    return nullptr;
}

