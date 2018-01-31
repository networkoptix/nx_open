#ifdef ENABLE_DLINK

#include "dlink_stream_reader.h"

#include <QtCore/QTextStream>

#include <nx/utils/log/log.h>
#include <utils/common/synctime.h>
#include <nx/network/http/http_types.h>

#include "nx/streaming/video_data_packet.h"
#include <nx/streaming/config.h>

extern int getIntParam(const char* pos);

#pragma pack(push,1)
struct ACS_VideoHeader
{
    quint32  ulHdrID; //Header ID
    quint32 ulHdrLength;
    quint32 ulDataLength;
    quint32 ulSequenceNumber;
    quint32 ulTimeSec;
    quint32 ulTimeUSec;
    quint32 ulDataCheckSum;
    quint16 usCodingType;
    quint16 usFrameRate;
    quint16 usWidth;
    quint16 usHeight;
    quint8 ucMDBitmap;
    quint8 ucMDPowers[3];
};

#pragma pack(pop)

PlDlinkStreamReader::PlDlinkStreamReader(const QnResourcePtr& res):
    CLServerPushStreamReader(res),
    m_rtpReader(res)
{

}

PlDlinkStreamReader::~PlDlinkStreamReader()
{
    stop();
}

QString PlDlinkStreamReader::getRTPurl(int profileId) const
{
    CLHttpStatus status;

    QnPlDlinkResourcePtr res = getResource().dynamicCast<QnPlDlinkResource>();
    QAuthenticator auth = res->getAuth();
    QByteArray reply = downloadFile(status, QString(lit("config/rtspurl.cgi?profileid=%1")).arg(profileId),  res->getHostAddress(), 80, 1000, auth);

    if (status != CL_HTTP_SUCCESS || reply.isEmpty())
        return QString();

    int lastProfileId = -1;
    QByteArray requiredProfile = QString(lit("profileid=%1")).arg(profileId).toUtf8();
    QList<QByteArray> lines = reply.split('\n');
    for(QByteArray line: lines)
    {
        line = line.toLower().trimmed();
        if (line.startsWith("profileid="))
            lastProfileId = line.split('=')[1].toInt();
        else if (line.startsWith("urlentry=") && lastProfileId == profileId)
            return QString::fromUtf8(line.split('=')[1]);
    }

    return QString();
}

CameraDiagnostics::Result PlDlinkStreamReader::openStreamInternal(bool isCameraControlRequired, const QnLiveStreamParams& params)
{
    if (isStreamOpened())
        return CameraDiagnostics::NoErrorResult();

    //setRole(Qn::CR_SecondaryLiveVideo);
    m_rtpReader.setRole(getRole());

    // ==== init if needed
    Qn::ConnectionRole role = getRole();
    QnPlDlinkResourcePtr res = getResource().dynamicCast<QnPlDlinkResource>();

    // ==== profile setup
    QString prifileStr = composeVideoProfile(isCameraControlRequired, params);

    if (prifileStr.isEmpty())
    {
        if (role == Qn::CR_SecondaryLiveVideo)
            qWarning() << "No dualstreaming for DLink camera " << m_resource->getUrl() << ". Ignore second url request";

        QUrl requestedUrl;
        requestedUrl.setHost( res->getHostAddress() );
        requestedUrl.setPort( 80 );
        requestedUrl.setScheme( QLatin1String("http") );
        return CameraDiagnostics::NoMediaTrackResult( requestedUrl.toString() );
    }

    QUrl requestedUrl;
    requestedUrl.setHost( res->getHostAddress() );
    requestedUrl.setPort( 80 );
    requestedUrl.setScheme( QLatin1String("http") );
    requestedUrl.setPath( prifileStr );

    CLHttpStatus status;
    QByteArray cam_info_file = downloadFile(status, prifileStr,  res->getHostAddress(), 80, 1000, res->getAuth()); // setup video profile

    if (status == CL_HTTP_AUTH_REQUIRED)
    {
        getResource()->setStatus(Qn::Unauthorized);
        return CameraDiagnostics::NotAuthorisedResult( requestedUrl.toString() );
    }

    if (cam_info_file.isEmpty())
    {
        return CameraDiagnostics::NoMediaTrackResult( requestedUrl.toString() );
    }

    if (isCameraControlRequired) {
        if (role != Qn::CR_SecondaryLiveVideo && res->getMotionType() != Qn::MT_SoftwareGrid)
            res->setMotionMaskPhysical(0);
    }

    // =====requesting a video
    QnDlink_cam_info info = res->getCamInfo();
    const int profileIndex = role == Qn::CR_SecondaryLiveVideo ? 1 : 0;
    if (profileIndex >= info.profiles.size() )
        return CameraDiagnostics::NoMediaTrackResult( requestedUrl.toString() );
    m_profile = info.profiles[profileIndex];

    if (m_profile.url.isEmpty())
    {
        qWarning() << "Invalid answer from DLink camera " << m_resource->getUrl() << ". Expecting non empty rtsl url.";
        return CameraDiagnostics::CameraResponseParseErrorResult( m_resource->getUrl(), lit("config/rtspurl.cgi?profileid=%1").arg(m_profile.url) );
    }

    NX_LOG(lit("got stream URL %1 for camera %2 for role %3").arg(m_profile.url).arg(m_resource->getUrl()).arg(getRole()), cl_logINFO);
    if (m_profile.codec.contains("264"))
    {
        QString rtspUrl(m_profile.url);
        if (!rtspUrl.contains(".sdp"))
            rtspUrl = getRTPurl(m_profile.number); //< DLink with old firmware. profile url contains some string, but it can't be passed to the RTSP

        m_rtpReader.setRequest(rtspUrl);
        res->updateSourceUrl(m_rtpReader.getCurrentStreamUrl(), getRole());
        return m_rtpReader.openStream();
    }
    else
    {
        // mpeg4 or jpeg
        m_HttpClient.reset(new CLSimpleHTTPClient(res->getHostAddress(), 80, 2000, res->getAuth()));
        const CLHttpStatus status = m_HttpClient->doGET(m_profile.url);
		if (status == CL_HTTP_SUCCESS)
		{
			QUrl httpStreamUrl;
			httpStreamUrl.setScheme("http");
			httpStreamUrl.setHost(res->getHostAddress());
			httpStreamUrl.setPort(80);
			httpStreamUrl.setPath(m_profile.url);
			res->updateSourceUrl(httpStreamUrl.toString(), getRole());
			return CameraDiagnostics::NoErrorResult();
		}
		else
		{
			return CameraDiagnostics::RequestFailedResult(m_profile.url, QLatin1String(nx::network::http::StatusCode::toString((nx::network::http::StatusCode::Value)status)));
		}
    }
}

void PlDlinkStreamReader::closeStream()
{
    m_HttpClient.reset();
    m_rtpReader.closeStream();
}

bool PlDlinkStreamReader::isStreamOpened() const
{
    return (m_HttpClient && m_HttpClient->isOpened()) || m_rtpReader.isStreamOpened();
}

QnAbstractMediaDataPtr PlDlinkStreamReader::getNextData()
{
    if (!isStreamOpened())
        return QnAbstractMediaDataPtr();

    if (needMetadata())
        return getMetadata();

    if (m_profile.codec.contains("264"))
        return m_rtpReader.getNextData();
    else if (m_profile.codec == "MJPEG")
        return getNextDataMJPEG();
    else
        return getNextDataMPEG(AV_CODEC_ID_MPEG4);
}

// =====================================================================================

int scaleInt(int value, int from, int to)
{
    double step = 1.0 / (double) (from-1);
    return int (step * value * (double) (to-1) + 0.5);
}

bool PlDlinkStreamReader::isTextQualities(const QList<QByteArray>& qualities) const
{
    if (qualities.isEmpty())
        return false;
    QByteArray val = qualities[0];
    for (int i = 0; i < val.length(); ++i)
    {
        bool isDigit = val.at(i) >= '0' && val.at(i) <= '9';
        if (!isDigit)
            return true;
    }

    return false;
}

QByteArray PlDlinkStreamReader::getQualityString(const QnLiveStreamParams& params) const
{
    QnPlDlinkResourcePtr res = getResource().dynamicCast<QnPlDlinkResource>();
    QnDlink_cam_info info = res->getCamInfo();

    if (info.possibleQualities.isEmpty())
    {
        int q;
        switch (params.quality)
        {
        case Qn::QualityHighest:
            q = 90;
            break;

        case Qn::QualityHigh:
            q = 80;
            break;

        case Qn::QualityNormal:
            q = 70;
            break;

        case Qn::QualityLow:
            q = 50;
            break;

        case Qn::QualityLowest:
            q = 40;
            break;

        default:
            q = 60;
            break;
        }
        return QByteArray::number(q);
    }
    int qualityIndex = scaleInt((int) params.quality, Qn::QualityHighest-Qn::QualityLowest+1, info.possibleQualities.size());
    if (isTextQualities(info.possibleQualities))
        qualityIndex = info.possibleQualities.size()-1 - qualityIndex; // index 0 is best quality if quality is text
    return info.possibleQualities[qualityIndex];
}

QString PlDlinkStreamReader::composeVideoProfile(bool isCameraControlRequired, const QnLiveStreamParams& streamParams)
{
    QnLiveStreamParams params = streamParams;

    QnPlDlinkResourcePtr res = getResource().dynamicCast<QnPlDlinkResource>();
    QnDlink_cam_info info = res->getCamInfo();

    Qn::ConnectionRole role = getRole();
    QnDlink_ProfileInfo profile;

    QSize resolution;

    if (role == Qn::CR_SecondaryLiveVideo)
    {
        if (info.profiles.size() < 2)
            return QString();
        profile = info.profiles[1];
        resolution = info.secondaryStreamResolution();
    }
    else
    {
        if (info.profiles.isEmpty())
            return QString();
        profile = info.profiles[0];
        //resolution = info.resolutionCloseTo(320);
        resolution = info.primaryStreamResolution();
    }
    m_resolution = resolution;
    // TODO: advanced params control is not implemented for this driver yet
    params.resolution = resolution;

    QString result;
    QTextStream t(&result);

    t << "config/video.cgi?profileid=" << profile.number;
    if (isCameraControlRequired)
    {
        t << "&resolution=" << resolution.width() << "x" << resolution.height() << "&";

        params.fps = info.frameRateCloseTo( qMin((int)params.fps, res->getMaxFps()) );
        t << "framerate=" << params.fps << "&";
        t << "codec=" << profile.codec.toLower() << "&";
        bool useCBR = (profile.codec.contains("264") || profile.codec == "MPEG4");
        if (useCBR)
        {
            // just CBR fo mpeg so far
            t << "qualitymode=CBR" << "&";
            t << "bitrate=" <<  info.bitrateCloseTo(res->suggestBitrateKbps(params, getRole()));
        }
        else
        {
            t << "qualitymode=Fixquality" << "&";
            t << "quality=" << getQualityString(params);
        }
    }

    t.flush();
    return result;
}

QnAbstractMediaDataPtr PlDlinkStreamReader::getNextDataMPEG(AVCodecID ci)
{
    char headerBuffer[sizeof(ACS_VideoHeader)];
    uint gotInBuffer = 0;
    while (gotInBuffer < sizeof(ACS_VideoHeader))
    {
        int readed = m_HttpClient->read(headerBuffer + gotInBuffer, sizeof(ACS_VideoHeader) - gotInBuffer);

        if (readed < 1)
            return QnAbstractMediaDataPtr(0);

        gotInBuffer += readed;
    }

    ACS_VideoHeader *vh = (ACS_VideoHeader*)(headerBuffer);
    if (vh->ulHdrID != 0xF5010000)
        return QnAbstractMediaDataPtr(0); // must be bad header

    if (vh->ulDataLength > 1024*1024*10)
        return QnAbstractMediaDataPtr(0); // to big video image


    int dataLeft = vh->ulDataLength;//qMin(vh->ulDataLength, (quint32)1024*1024*10); // to avoid crash


    QnWritableCompressedVideoDataPtr videoData(new QnWritableCompressedVideoData(CL_MEDIA_ALIGNMENT, dataLeft+FF_INPUT_BUFFER_PADDING_SIZE));
    char* curPtr = videoData->m_data.data();
    videoData->m_data.startWriting(dataLeft); // this call does nothing
    videoData->m_data.finishWriting(dataLeft);

    while (dataLeft > 0)
    {
        int readed = m_HttpClient->read(curPtr, dataLeft);
        if (readed < 1)
            return QnAbstractMediaDataPtr(0);
        curPtr += readed;
        dataLeft -= readed;
    }


    videoData->compressionType = ci;
    videoData->width = vh->usWidth;
    videoData->height = vh->usWidth;

    if (vh->usCodingType==0)
        videoData->flags |= QnAbstractMediaData::MediaFlags_AVKey;

    videoData->channelNumber = 0;
    videoData->timestamp = qnSyncTime->currentMSecsSinceEpoch() * 1000;

    return videoData;

}


QnAbstractMediaDataPtr PlDlinkStreamReader::getNextDataMJPEG()
{
    char headerBuffer[512+1];
    uint headerSize = 0;
    char* headerBufferEnd = 0;
    char* realHeaderEnd = 0;
    while (headerSize < sizeof(headerBuffer)-1)
    {
        int readed = m_HttpClient->read(headerBuffer+headerSize, sizeof(headerBuffer)-1 - headerSize);
        if (readed < 1)
            return QnAbstractMediaDataPtr(0);
        headerSize += readed;
        headerBufferEnd = headerBuffer + headerSize;
        *headerBufferEnd = 0;
        realHeaderEnd = strstr(headerBuffer, "\r\n\r\n");
        if (realHeaderEnd)
            break;
    }
    if (!realHeaderEnd)
        return QnAbstractMediaDataPtr(0);
    char* contentLenPtr = strstr(headerBuffer, "Content-Length:");
    if (!contentLenPtr)
        return QnAbstractMediaDataPtr(0);


    int contentLen = getIntParam(contentLenPtr + 16);
    contentLen = qMin(contentLen, 10*1024*1024); // to avoid crash if camera is crazy

    QnWritableCompressedVideoDataPtr videoData(new QnWritableCompressedVideoData(CL_MEDIA_ALIGNMENT, contentLen+FF_INPUT_BUFFER_PADDING_SIZE));
    videoData->m_data.write(realHeaderEnd+4, headerBufferEnd - (realHeaderEnd+4));

    int dataLeft = contentLen - static_cast<int>(videoData->dataSize());
    char* curPtr = videoData->m_data.data() + videoData->m_data.size();
    videoData->m_data.finishWriting(dataLeft);

    while (dataLeft > 0)
    {
        int readed = m_HttpClient->read(curPtr, dataLeft);
        if (readed < 1)
            return QnAbstractMediaDataPtr(0);
        curPtr += readed;
        dataLeft -= readed;
    }
    // sometime 1 more bytes in the buffer end. Looks like it is a DLink bug caused by 16-bit word alignment
    if (contentLen > 2 && !(curPtr[-2] == (char)0xff && curPtr[-1] == (char)0xd9))
        videoData->m_data.finishWriting(-1);

    videoData->compressionType = AV_CODEC_ID_MJPEG;
    videoData->width = m_resolution.width();
    videoData->height = m_resolution.height();
    videoData->flags |= QnAbstractMediaData::MediaFlags_AVKey;
    videoData->channelNumber = 0;
    videoData->timestamp = qnSyncTime->currentMSecsSinceEpoch() * 1000;

    return videoData;

}


QnMetaDataV1Ptr PlDlinkStreamReader::getCameraMetadata()
{

    QnPlDlinkResourcePtr res = getResource().dynamicCast<QnPlDlinkResource>();


    CLHttpStatus status;
    QByteArray cam_info_file = downloadFile(status, QLatin1String("config/notify.cgi"),  res->getHostAddress(), 80, 1000, res->getAuth());

    if (status == CL_HTTP_AUTH_REQUIRED)
    {
        res->setStatus(Qn::Unauthorized);
        return QnMetaDataV1Ptr(0);
    }


    if (cam_info_file.size()==0)
        return QnMetaDataV1Ptr(new QnMetaDataV1());

    QString file_s = QLatin1String(cam_info_file);
    QStringList lines = file_s.split(QLatin1String("\r\n"), QString::SkipEmptyParts);

    bool empty = true;

    for(const QString& line: lines)
    {
        QString line_low = line.toLower();

        if (line_low.contains(QLatin1String("md0")) || line_low.contains(QLatin1String("md1")) || line_low.contains(QLatin1String("md2")))
        {

            if (line_low.contains(QLatin1String("on")))
                empty = false;
        }
    }


    QnMetaDataV1Ptr motion;

    if (empty)
        motion = QnMetaDataV1Ptr(new QnMetaDataV1());
    else
        motion = QnMetaDataV1Ptr(new QnMetaDataV1(1));


    //motion->m_duration = META_DATA_DURATION_MS * 1000 ;
    motion->m_duration = 1000*1000*1000; // 1000 sec
    filterMotionByMask(motion);
    return motion;
}

void PlDlinkStreamReader::pleaseStop()
{
    CLServerPushStreamReader::pleaseStop();
    m_rtpReader.pleaseStop();
}


#endif // #ifdef ENABLE_DLINK
