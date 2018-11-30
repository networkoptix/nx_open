#include "wearable_camera_resource.h"

#include <core/resource/avi/avi_resource.h>
#include <nx/streaming/abstract_archive_stream_reader.h>
#include <nx/streaming/abstract_archive_delegate.h>

const QString QnWearableCameraResource::kManufacture = lit("WEARABLE_CAMERA");
using namespace nx::vms::server::resource;

QnWearableCameraResource::QnWearableCameraResource(QnMediaServerModule* serverModule):
    nx::vms::server::resource::Camera(serverModule)
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

    setProperty(ResourcePropertyKey::kIsAudioSupported, lit("1"));
    setProperty(ResourcePropertyKey::kHasDualStreaming, lit("0"));
    setProperty(ResourcePropertyKey::kSupportedMotion, lit("softwaregrid"));
    saveProperties();

    return result;
}

QString QnWearableCameraResource::getDriverName() const
{
    return kManufacture;
}

void QnWearableCameraResource::setStatus(Qn::ResourceStatus, Qn::StatusChangeReason reason)
{
    base_type::setStatus(Qn::Online, reason);
}

QnConstResourceAudioLayoutPtr QnWearableCameraResource::getAudioLayout(
    const QnAbstractStreamDataProvider* dataProvider) const
{
    if (!dataProvider)
        return QnConstResourceAudioLayoutPtr();

    const QnAbstractArchiveStreamReader* reader =
        dynamic_cast<const QnAbstractArchiveStreamReader*>(dataProvider);
    if (reader && reader->getArchiveDelegate())
        return reader->getArchiveDelegate()->getAudioLayout();

    QnAviResourcePtr resource = dataProvider->getResource().dynamicCast<QnAviResource>();
    if(resource)
        return resource->getAudioLayout(dataProvider);

    NX_ASSERT(false);
    return QnConstResourceAudioLayoutPtr();
}

QnAbstractStreamDataProvider* QnWearableCameraResource::createLiveDataProvider()
{
    return nullptr;
}

StreamCapabilityMap QnWearableCameraResource::getStreamCapabilityMapFromDrives(Qn::StreamIndex /*streamIndex*/)
{
    return StreamCapabilityMap();
}

CameraDiagnostics::Result QnWearableCameraResource::initializeCameraDriver()
{
    return CameraDiagnostics::NoErrorResult();
}
