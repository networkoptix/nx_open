#include "client_camera.h"

#include <plugins/resource/archive/archive_stream_reader.h>
#include <plugins/resource/archive/rtsp_client_archive_delegate.h>

QnClientCameraResource::QnClientCameraResource(const QnUuid &resourceTypeId)
    : base_type(resourceTypeId)
{
}

Qn::ResourceFlags QnClientCameraResource::flags() const {
    Qn::ResourceFlags result = base_type::flags();
    if (!isDtsBased() && supportedMotionType() != Qn::MT_NoMotion)
        result |= Qn::motion;
    return result;
}

QnConstResourceVideoLayoutPtr QnClientCameraResource::getVideoLayout(const QnAbstractStreamDataProvider *dataProvider) const {
    return QnVirtualCameraResource::getVideoLayout(dataProvider);
}

QnConstResourceAudioLayoutPtr QnClientCameraResource::getAudioLayout(const QnAbstractStreamDataProvider *dataProvider) const {
    const QnArchiveStreamReader *archive = dynamic_cast<const QnArchiveStreamReader*> (dataProvider);
    if (archive)
        return archive->getDPAudioLayout();
    else
        return QnMediaResource::getAudioLayout();
}


QnAbstractStreamDataProvider *QnClientCameraResource::createLiveDataProvider() {
    QnArchiveStreamReader *result = new QnArchiveStreamReader(toSharedPointer());
    result->setArchiveDelegate(new QnRtspClientArchiveDelegate(result));
    return result;
}
