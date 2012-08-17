#include <QTextStream>
#include "isd_resource.h"
#include "isd_stream_reader.h"
#include "utils/common/sleep.h"
#include "utils/common/synctime.h"
#include "utils/media/nalUnits.h"
#include "utils/network/simple_http_client.h"

extern QString getValueFromString(const QString& line);

QnISDStreamReader::QnISDStreamReader(QnResourcePtr res):
    CLServerPushStreamreader(res),
    QnLiveStreamProvider(res),
    m_rtpStreamParser(res)
{
    //m_axisRes = getResource().dynamicCast<QnPlAxisResource>();
}

QnISDStreamReader::~QnISDStreamReader()
{
    closeStream();
}


void QnISDStreamReader::openStream()
{
    if (isStreamOpened())
        return;

    QnResource::ConnectionRole role = getRole();
    QnPlIsdResourcePtr res = getResource().dynamicCast<QnPlIsdResource>();


    CLSimpleHTTPClient http (res->getHostAddress(), 80, 3000, res->getAuth());
    QByteArray request;
    QString result;
    QTextStream t(&result);

    if (role == QnResource::Role_SecondaryLiveVideo)
    {
        t << "VideoInput.1.h264.2.Resolution=" << res->getSecondaryResolution().width() << "x" << res->getSecondaryResolution().height() << "\r\n";
        t << "VideoInput.1.h264.2.FrameRate=5" << "\r\n";
        t << "VideoInput.1.h264.2.BitRate=128" << "\r\n";
    }
    else
    {
        t << "VideoInput.1.h264.1.Resolution=" << res->getPrimaryResolution().width() << "x" << res->getPrimaryResolution().height() << "\r\n";
        t << "VideoInput.1.h264.1.FrameRate=" << getFps() << "\r\n";
        t << "VideoInput.1.h264.1.BitRate=" << res->suggestBitrateKbps(getQuality(), res->getPrimaryResolution(), getFps()) << "\r\n";
    }


    
    request.append(result.toLatin1());
    CLHttpStatus status = http.doPOST(QByteArray("/api/param.cgi"), QLatin1String(request));
    QnSleep::msleep(100);

    QString urlrequest = (role == QnResource::Role_SecondaryLiveVideo)
        ? QLatin1String("api/param.cgi?req=VideoInput.1.h264.2.Rtsp.AbsolutePath")
        : QLatin1String("api/param.cgi?req=VideoInput.1.h264.1.Rtsp.AbsolutePath");

    QByteArray reslst = downloadFile(status, urlrequest,  res->getHostAddress(), 80, 3000, res->getAuth());
    if (status == CL_HTTP_AUTH_REQUIRED)
    {
        res->setStatus(QnResource::Unauthorized);
        return;
    }

    QString url = getValueFromString(QLatin1String(reslst));

    QStringList urlLst = url.split(QLatin1Char('\r'), QString::SkipEmptyParts);
    if(urlLst.size() < 1)
        return;

    url = urlLst.at(0);
    

    if (url.isEmpty())
        return;


    m_rtpStreamParser.setRequest(url);
    m_rtpStreamParser.openStream();
}

void QnISDStreamReader::closeStream()
{
    m_rtpStreamParser.closeStream();
}

bool QnISDStreamReader::isStreamOpened() const
{
    return m_rtpStreamParser.isStreamOpened();
}

QnMetaDataV1Ptr QnISDStreamReader::getCameraMetadata()
{
    return QnMetaDataV1Ptr(0);
}



void QnISDStreamReader::pleaseStop()
{
    QnLongRunnable::pleaseStop();
    m_rtpStreamParser.closeStream();
}

QnAbstractMediaDataPtr QnISDStreamReader::getNextData()
{
    if (!isStreamOpened()) {
        openStream();
        if (!isStreamOpened())
            return QnAbstractMediaDataPtr(0);
    }

    if (needMetaData())
        return getMetaData();

    QnAbstractMediaDataPtr rez;
    int errorCount = 0;
    for (int i = 0; i < 10; ++i)
    {
        rez = m_rtpStreamParser.getNextData();
        if (rez) 
        {
            QnCompressedVideoDataPtr videoData = qSharedPointerDynamicCast<QnCompressedVideoData>(rez);
            //ToDo: if (videoData)
            //    parseMotionInfo(videoData);

            //if (!videoData || isGotFrame(videoData))
            break;
        }
        else {
            errorCount++;
            if (errorCount > 1) {
                closeStream();
                break;
            }
        }
    }

    return rez;
}

void QnISDStreamReader::updateStreamParamsBasedOnQuality()
{
    if (isRunning())
        pleaseReOpen();
}

void QnISDStreamReader::updateStreamParamsBasedOnFps()
{
    if (isRunning())
        pleaseReOpen();
}

const QnResourceAudioLayout* QnISDStreamReader::getDPAudioLayout() const
{
    return m_rtpStreamParser.getAudioLayout();
}
