#include "wearable_camera_resource.h"

#include <plugins/resource/avi/avi_resource.h>

const QString QnWearableCameraResource::kManufacture = lit("WEARABLE_CAMERA");

QnWearableCameraResource::QnWearableCameraResource()
{
    removeFlags(Qn::live | Qn::network | Qn::streamprovider | Qn::motion);
    addFlags(Qn::wearable_camera);
}

QnWearableCameraResource::~QnWearableCameraResource()
{
}

CameraDiagnostics::Result QnWearableCameraResource::initInternal()
{
    CameraDiagnostics::Result result = base_type::initInternal();

    setProperty(Qn::IS_AUDIO_SUPPORTED_PARAM_NAME, lit("1"));
    setProperty(Qn::HAS_DUAL_STREAMING_PARAM_NAME, lit("0"));
#ifdef ENABLE_SOFTWARE_MOTION_DETECTION
    setProperty(Qn::SUPPORTED_MOTION_PARAM_NAME, lit("softwaregrid"));
#else
    setProperty(Qn::SUPPORTED_MOTION_PARAM_NAME, lit("none"));
#endif
    saveParams();

    return result;
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

QnConstResourceAudioLayoutPtr QnWearableCameraResource::getAudioLayout(
    const QnAbstractStreamDataProvider* dataProvider) const
{
    if (!dataProvider)
        return QnConstResourceAudioLayoutPtr();

    QnAviResourcePtr resource = dataProvider->getResource().dynamicCast<QnAviResource>();
    if(!resource)
        return QnConstResourceAudioLayoutPtr();

    return resource->getAudioLayout(dataProvider);
}

QnAbstractStreamDataProvider* QnWearableCameraResource::createLiveDataProvider()
{
    return nullptr;
}

