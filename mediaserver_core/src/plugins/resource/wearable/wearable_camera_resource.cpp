#include "wearable_camera_resource.h"

const QString QnWearableCameraResource::MANUFACTURE = lit("WEARABLE_CAMERA");

QnWearableCameraResource::QnWearableCameraResource() {
    removeFlags(Qn::live | Qn::network | Qn::streamprovider);
}

QnWearableCameraResource::~QnWearableCameraResource() {}

QString QnWearableCameraResource::getDriverName() const {
    return MANUFACTURE;
}

QnAbstractStreamDataProvider* QnWearableCameraResource::createLiveDataProvider() {
    return nullptr; /* No live data! */
}

