#include "digital_watchdog_stream_reader.h"
#include "digital_watchdog_resource.h"


QnPlDWDStreamReader::QnPlDWDStreamReader(QnResourcePtr res):
    RTP264StreamReader(res)
{

}

QnPlDWDStreamReader::~QnPlDWDStreamReader()
{

}

void QnPlDWDStreamReader::openStream() 
{
    
    QnPlWatchDogResourcePtr res = getResource().dynamicCast<QnPlWatchDogResource>();
    int fps = qMin((int)getFps(), res->getMaxFps());
    int kbps = bestBitrateKbps(getQuality(), QSize(1920,180), fps);

    kbps = qMin(kbps, 8000);
    kbps = qMax(kbps, 2000);

    QString requst;
    QTextStream ts(&requst);

    //ts << "cameraname1=1CH&cameraname2=2CH&resolution1=1920:1080&resolution2=960:544&codec1=h264&codec2=mjpeg&bitrate1=" << kbps << "&bitrate2=4000&framerate1=" << fps << "&framerate2=30&quality1=2&quality2=2";

    ts << "cameraname1=1CH&cameraname2=2CH&resolution1=1920:1080&resolution2=960:544&codec1=h264&codec2=mjpeg&bitrate1=4000&bitrate2=4000&framerate1=30&framerate2=30&quality1=2&quality2=2";

    
    CLHttpStatus status;
    CLSimpleHTTPClient http (res->getHostAddress(), 80, 1000, res->getAuth());
    status = http.doPOST(QString("cgi-bin/camerasetup.cgi").toUtf8(), requst);

    

    if (status == CL_HTTP_AUTH_REQUIRED)
    {
        res->setStatus(QnResource::Unauthorized);
        return;
    }
    /**/

    RTP264StreamReader::setRequest("h264");
    RTP264StreamReader::openStream();
}

void QnPlDWDStreamReader::updateStreamParamsBasedOnQuality()
{
    if (isRunning())
        pleaseReOpen();
};

void QnPlDWDStreamReader::updateStreamParamsBasedOnFps() 
{
    if (isRunning())
        pleaseReOpen();
};

