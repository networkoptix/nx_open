#ifdef ENABLE_ISD

#include "isd_stream_reader.h"

#include <QtCore/QTextStream>

#include <nx/utils/log/log.h>
#include <utils/common/sleep.h>
#include <nx/network/deprecated/simple_http_client.h>

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

QString QnISDStreamReader::serializeStreamParams(
    const QnLiveStreamParams& params,
    const QSize& resolution,
    int profileIndex) const
{
    QnPlIsdResourcePtr res = getResource().dynamicCast<QnPlIsdResource>();

    QString result;
    QTextStream t(&result);

    static const int kMinBitrate = 256; //< kbps
    const int desiredBitrateKbps = std::max(kMinBitrate, res->suggestBitrateKbps(resolution, params, getRole()));

    t << "VideoInput.1.h264." << profileIndex << ".Resolution=" << resolution.width()
      << "x" << resolution.height() << "\r\n";
    t << "VideoInput.1.h264." << profileIndex << ".FrameRate=" << params.fps << "\r\n";
    t << "VideoInput.1.h264." << profileIndex << ".BitrateControl=vbr\r\n";
    t << "VideoInput.1.h264." << profileIndex << ".BitrateVariableMin=" << desiredBitrateKbps / 5 << "\r\n";
    t << "VideoInput.1.h264." << profileIndex << ".BitrateVariableMax=" << desiredBitrateKbps / 5 * 6 << "\r\n";    //*1.2
    t.flush();
    return result;
}

CameraDiagnostics::Result QnISDStreamReader::openStreamInternal(bool isCameraControlRequired, const QnLiveStreamParams& params)
{
    if (isStreamOpened())
        return CameraDiagnostics::NoErrorResult();

    Qn::ConnectionRole role = getRole();
    m_rtpStreamParser.setRole(role);
    QnPlIsdResourcePtr res = getResource().dynamicCast<QnPlIsdResource>();
    CLHttpStatus status;

    int port = QUrl(res->getUrl()).port(nx::network::http::DEFAULT_HTTP_PORT);
    CLSimpleHTTPClient http (res->getHostAddress(), port, ISD_HTTP_REQUEST_TIMEOUT_MS, res->getAuth());

    QSize resolution;
    int profileIndex;
    if (role == Qn::CR_SecondaryLiveVideo)
    {
        resolution = res->getSecondaryResolution();
        profileIndex = 2;
    }
    else
    {
        resolution = res->getPrimaryResolution();
        profileIndex = 1;
    }

    if (isCameraControlRequired)
    {
        QString streamProfileStr = serializeStreamParams(params, resolution, profileIndex);
        status = http.doPOST(QByteArray("/api/param.cgi"), streamProfileStr);
        QnSleep::msleep(100);
    }

    QString urlrequest =
        lit("api/param.cgi?req=VideoInput.1.h264.%1.Rtsp.AbsolutePath").arg(profileIndex);

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

    res->updateSourceUrl(url, getRole());
    NX_LOG(lit("got stream URL %1 for camera %2 for role %3").arg(url).arg(m_resource->getUrl()).arg(getRole()), cl_logINFO);

    //m_resource.dynamicCast<QnNetworkResource>()->setMediaPort(8554);

    m_rtpStreamParser.setRequest(url);
	res->updateSourceUrl(m_rtpStreamParser.getCurrentStreamUrl(), getRole());
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
    if (!isStreamOpened())
        return QnAbstractMediaDataPtr(0);

    if (needMetadata())
        return getMetadata();

    QnAbstractMediaDataPtr rez;
    int errorCount = 0;
    for (int i = 0; i < 10; ++i)
    {
        rez = m_rtpStreamParser.getNextData();
        if (rez)
        {
            //QnCompressedVideoDataPtr videoData = std::dynamic_pointer_cast<QnCompressedVideoData>(rez);
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

QnConstResourceAudioLayoutPtr QnISDStreamReader::getDPAudioLayout() const
{
    return m_rtpStreamParser.getAudioLayout();
}

#endif // #ifdef ENABLE_ISD
