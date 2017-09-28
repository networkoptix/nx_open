#include "client_camera.h"

#include <nx/streaming/archive_stream_reader.h>
#include <nx/streaming/rtsp_client_archive_delegate.h>

#include <nx/client/desktop/radass/radass_controller.h>

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


QnAbstractStreamDataProvider* QnClientCameraResource::createLiveDataProvider()
{
    QnArchiveStreamReader *result = new QnArchiveStreamReader(toSharedPointer());
    auto delegate = new QnRtspClientArchiveDelegate(result);
    result->setArchiveDelegate(delegate);

    connect(delegate, &QnRtspClientArchiveDelegate::dataDropped, this,
        &QnClientCameraResource::dataDropped);

    return result;
}
