#include <QTextStream>
#include "desktop_camera_reader.h"
#include "utils/common/sleep.h"
#include "utils/network/tcp_connection_priv.h"

QnDesktopCameraStreamReader::QnDesktopCameraStreamReader(QnResourcePtr res):
    CLServerPushStreamReader(res)
{
}

QnDesktopCameraStreamReader::~QnDesktopCameraStreamReader()
{
    stop();
}

CameraDiagnostics::Result QnDesktopCameraStreamReader::openStream()
{
    const CameraDiagnostics::Result result;
    return result;
}

void QnDesktopCameraStreamReader::closeStream()
{
}

bool QnDesktopCameraStreamReader::isStreamOpened() const
{
    return false;
}


void QnDesktopCameraStreamReader::pleaseStop()
{
    QnLongRunnable::pleaseStop();
}

QnAbstractMediaDataPtr QnDesktopCameraStreamReader::getNextData()
{
    if (!isStreamOpened()) {
        openStream();
        if (!isStreamOpened())
            return QnAbstractMediaDataPtr(0);
    }

    return QnAbstractMediaDataPtr(0);
}

void QnDesktopCameraStreamReader::updateStreamParamsBasedOnQuality()
{
    if (isRunning())
        pleaseReOpen();
}

void QnDesktopCameraStreamReader::updateStreamParamsBasedOnFps()
{
    if (isRunning())
        pleaseReOpen();
}
