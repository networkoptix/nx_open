#include "fake_camera.h"
#include "plugins/resource/archive/archive_stream_reader.h"
#include "plugins/resource/archive/avi_files/avi_archive_delegate.h"

QnFakeCamera::QnFakeCamera()
{
    addFlags(server_live_cam);
}

QnAbstractStreamDataProvider* QnFakeCamera::createDataProvider(ConnectionRole /*role*/)
{
    QnArchiveStreamReader* result = new QnArchiveStreamReader(toSharedPointer());
    result->setArchiveDelegate(new QnAviArchiveDelegate());
    return result;
}
