#include "server_camera.h"
#include "plugins/resource/archive/archive_stream_reader.h"
#include "plugins/resource/archive/rtsp_client_archive_delegate.h"
#include "core/resource_management/resource_pool.h"
#include "core/resource/media_server_resource.h"
#include "api/app_server_connection.h"


QnServerCamera::QnServerCamera(const QnId& resourceTypeId): QnVirtualCameraResource()
{
    setTypeId(resourceTypeId);
    addFlags(server_live_cam);
    if (!isDtsBased() && supportedMotionType() != Qn::MT_NoMotion)
        addFlags(QnResource::motion);
    m_tmpStatus = NotDefined;
}

bool QnServerCamera::isResourceAccessible()
{
    return true;
}

bool QnServerCamera::updateMACAddress()
{
    return true;
}

QString QnServerCamera::getDriverName() const
{
    return QLatin1String("Server camera"); //all other manufacture are also untranslated and should not be translated
}

void QnServerCamera::setIframeDistance(int frames, int timems)
{
    Q_UNUSED(frames)
    Q_UNUSED(timems)
}

QnConstResourceVideoLayoutPtr QnServerCamera::getVideoLayout(const QnAbstractStreamDataProvider* dataProvider) const
{
    Q_UNUSED(dataProvider)
    // todo: layout must be loaded in resourceParams
    return QnMediaResource::getVideoLayout();
}

QnConstResourceAudioLayoutPtr QnServerCamera::getAudioLayout(const QnAbstractStreamDataProvider* dataProvider) const
{
    const QnArchiveStreamReader* archive = dynamic_cast<const QnArchiveStreamReader*> (dataProvider);
    if (archive)
        return archive->getDPAudioLayout();
    else
        return QnMediaResource::getAudioLayout();
}


QnAbstractStreamDataProvider* QnServerCamera::createLiveDataProvider()
{
    QnArchiveStreamReader* result = new QnArchiveStreamReader(toSharedPointer());
    result->setArchiveDelegate(new QnRtspClientArchiveDelegate(result));
    return result;
}

QnResource::Status QnServerCamera::getStatus() const
{
    if (m_tmpStatus != NotDefined)
        return m_tmpStatus;
    else
        return QnResource::getStatus();
}

void QnServerCamera::setTmpStatus(Status value)
{
    if (value != m_tmpStatus) {
        Status oldStatus = getStatus();
        m_tmpStatus = value;
        Status newStatus = getStatus();
        if (oldStatus != newStatus)
            emit statusChanged(toSharedPointer(this));
    }
}
