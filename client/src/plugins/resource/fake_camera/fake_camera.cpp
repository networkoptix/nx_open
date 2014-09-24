#include "fake_camera.h"

#include "plugins/resource/archive/archive_stream_reader.h"
#include "plugins/resource/avi/avi_archive_delegate.h"

QnFakeCamera::QnFakeCamera()
{
    addFlags(Qn::server_live_cam);
}

QnAbstractStreamDataProvider* QnFakeCamera::createDataProvider(Qn::ConnectionRole /*role*/)
{
    QnArchiveStreamReader* result = new QnArchiveStreamReader(toSharedPointer());
    result->setArchiveDelegate(new QnAviArchiveDelegate());
    return result;
}
