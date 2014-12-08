#ifdef ENABLE_ISD

#include "isd_stream_reader.h"

#include <QtCore/QTextStream>

#include <utils/common/log.h>
#include <utils/common/sleep.h>
#include <utils/network/simple_http_client.h>

#include "isd_resource.h"


const int QnISDStreamReader::ISD_HTTP_REQUEST_TIMEOUT_MS = 5000;

extern QString getValueFromString(const QString& line);

QnISDStreamReader::QnISDStreamReader(const QnResourcePtr& res):
    CLServerPushStreamReader(res),
    m_rtpStreamParser(res)
{
    //m_axisRes = getResource().dynamicCast<QnPlAxisResource>();
}

QnISDStreamReader::~QnISDStreamReader()
{
    stop();
}

static const int BYTES_PER_KB = 1024;

#define USE_VBR

CameraDiagnostics::Result QnISDStreamReader::openStream()
{
    if (isStreamOpened())
        return CameraDiagnostics::NoErrorResult();

    Qn::ConnectionRole role = getRole();
    m_rtpStreamParser.setRole(role);
    QnPlIsdResourcePtr res = getResource().dynamicCast<QnPlIsdResource>();
    CLHttpStatus status;

    int port = QUrl(res->getUrl()).port(nx_http::DEFAULT_HTTP_PORT);
    CLSimpleHTTPClient http (res->getHostAddress(), port, ISD_HTTP_REQUEST_TIMEOUT_MS, res->getAuth());

    if (!isCameraControlDisabled())
    {
        QByteArray request;
        QString result;
        QTextStream t(&result);

        //const QnSecurityCamResource* cameraRes = dynamic_cast<const QnSecurityCamResource*>(m_resource.data());
        //Q_ASSERT( cameraRes );
        //const bool isAmbarellaFirmware = cameraRes->getModel().indexOf(lit("4K")) != -1;
        const bool isAmbarellaChipset = true; 

        if (role == Qn::CR_SecondaryLiveVideo)
        {
            const float fps = 5.0;
            const int desiredBitrateKbps = 512;

            t << "VideoInput.1.h264.2.Resolution=" << res->getSecondaryResolution().width() << 
                "x" << res->getSecondaryResolution().height() << "\r\n";
            t << "VideoInput.1.h264.2.FrameRate=" << fps << "\r\n";
#ifndef USE_VBR
            int constantBitrateToSetKbps = isAmbarellaChipset
                ? desiredBitrateKbps * 30 / fps     //constant bitrate MUST be multiplied by 30/fps
                : desiredBitrateKbps;
            t << "VideoInput.1.h264.2.BitrateControl=cbr\r\n";
            t << "VideoInput.1.h264.2.BitRate=" << constantBitrateToSetKbps << "\r\n";
#else
            t << "VideoInput.1.h264.2.BitrateControl=vbr\r\n";
            t << "VideoInput.1.h264.2.BitrateVariableMin=" << desiredBitrateKbps * BYTES_PER_KB / 5 << "\r\n";
            t << "VideoInput.1.h264.2.BitrateVariableMax=" << desiredBitrateKbps * BYTES_PER_KB / 5 * 6 << "\r\n";    //*1.2
#endif
        }
        else
        {
            const float fps = getFps();
            const QSize resolution = res->getPrimaryResolution();
            const int desiredBitrateKbps = res->suggestBitrateKbps(getQuality(), resolution, fps);
            t << "VideoInput.1.h264.1.Resolution=" << resolution.width() << "x" << resolution.height() << "\r\n";
            t << "VideoInput.1.h264.1.FrameRate=" << fps << "\r\n";
#ifndef USE_VBR
            int constantBitrateToSetKbps = isAmbarellaChipset
                ? desiredBitrateKbps * 30 / fps
                : desiredBitrateKbps;
            t << "VideoInput.1.h264.1.BitrateControl=cbr\r\n";
            t << "VideoInput.1.h264.1.BitRate=" << constantBitrateToSetKbps << "\r\n";
#else
            //TODO #ak ISD recommends switching to VBR
            t << "VideoInput.1.h264.1.BitrateControl=vbr\r\n";
            t << "VideoInput.1.h264.1.BitrateVariableMin=" << desiredBitrateKbps * BYTES_PER_KB / 5 << "\r\n";
            t << "VideoInput.1.h264.1.BitrateVariableMax=" << desiredBitrateKbps * BYTES_PER_KB / 5 * 6 << "\r\n";    //*1.2
#endif
        }
        t.flush();

        
        request.append(result.toLatin1());
        status = http.doPOST(QByteArray("/api/param.cgi"), QLatin1String(request));
        QnSleep::msleep(100);
    }

    QString urlrequest = (role == Qn::CR_SecondaryLiveVideo)
        ? QLatin1String("api/param.cgi?req=VideoInput.1.h264.2.Rtsp.AbsolutePath")
        : QLatin1String("api/param.cgi?req=VideoInput.1.h264.1.Rtsp.AbsolutePath");

    QByteArray reslst = downloadFile(status, urlrequest,  res->getHostAddress(), port, ISD_HTTP_REQUEST_TIMEOUT_MS, res->getAuth());
    if (status == CL_HTTP_AUTH_REQUIRED)
    {
        res->setStatus(Qn::Unauthorized);
        QUrl requestedUrl;
        requestedUrl.setHost( res->getHostAddress() );
        requestedUrl.setPort( port );
        requestedUrl.setScheme( QLatin1String("http") );
        requestedUrl.setPath( urlrequest );
        return CameraDiagnostics::NotAuthorisedResult( requestedUrl.toString() );
    }

    QString url = getValueFromString(QLatin1String(reslst));

    QStringList urlLst = url.split(QLatin1Char('\r'), QString::SkipEmptyParts);
    if(urlLst.size() < 1)
    {
        QUrl requestedUrl;
        requestedUrl.setHost( res->getHostAddress() );
        requestedUrl.setPort( port );
        requestedUrl.setScheme( QLatin1String("http") );
        requestedUrl.setPath( urlrequest );
        return CameraDiagnostics::NoMediaTrackResult( requestedUrl.toString() );
    }

    url = urlLst.at(0);
    

    if (url.isEmpty())
    {
        QUrl requestedUrl;
        requestedUrl.setHost( res->getHostAddress() );
        requestedUrl.setPort( port );
        requestedUrl.setScheme( QLatin1String("http") );
        requestedUrl.setPath( urlrequest );
        return CameraDiagnostics::NoMediaTrackResult( requestedUrl.toString() );
    }

    NX_LOG(lit("got stream URL %1 for camera %2 for role %3").arg(url).arg(m_resource->getUrl()).arg(getRole()), cl_logINFO);

    //m_resource.dynamicCast<QnNetworkResource>()->setMediaPort(8554);

    m_rtpStreamParser.setRequest(url);
    return m_rtpStreamParser.openStream();
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
    CLServerPushStreamReader::pleaseStop();
    m_rtpStreamParser.pleaseStop();
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

QnConstResourceAudioLayoutPtr QnISDStreamReader::getDPAudioLayout() const
{
    return m_rtpStreamParser.getAudioLayout();
}

#endif // #ifdef ENABLE_ISD
