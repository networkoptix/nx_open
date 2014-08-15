#ifdef ENABLE_DLINK

#include "dlink_stream_reader.h"

#include <QtCore/QTextStream>

#include <utils/common/log.h>
#include <utils/common/synctime.h>
#include <utils/network/http/httptypes.h>

#include "core/datapacket/video_data_packet.h"

#include "dlink_resource.h"


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
m_rtpReader(res),
mHttpClient(0),
m_h264(false),
m_mpeg4(false)
{
    
}

PlDlinkStreamReader::~PlDlinkStreamReader()
{
    stop();
}


CameraDiagnostics::Result PlDlinkStreamReader::openStream()
{
    if (isStreamOpened())
        return CameraDiagnostics::NoErrorResult();

    //setRole(Qn::CR_SecondaryLiveVideo);
    m_rtpReader.setRole(getRole());

    //==== init if needed
    Qn::ConnectionRole role = getRole();
    QnPlDlinkResourcePtr res = getResource().dynamicCast<QnPlDlinkResource>();
    if (!res->getCamInfo().inited())
    {
        res->init();
    }

    //==== profile setup
    QString prifileStr = composeVideoProfile();

    if (prifileStr.length()==0)
    {
        QUrl requestedUrl;
        requestedUrl.setHost( res->getHostAddress() );
        requestedUrl.setPort( 80 );
        requestedUrl.setScheme( QLatin1String("http") );
        return CameraDiagnostics::NoMediaTrackResult( requestedUrl.toString() );
    }

    CLHttpStatus status;
    QByteArray cam_info_file = downloadFile(status, prifileStr,  res->getHostAddress(), 80, 1000, res->getAuth()); // setup video profile

    if (status == CL_HTTP_AUTH_REQUIRED)
    {
        getResource()->setStatus(Qn::Unauthorized);
        QUrl requestedUrl;
        requestedUrl.setHost( res->getHostAddress() );
        requestedUrl.setPort( 80 );
        requestedUrl.setScheme( QLatin1String("http") );
        requestedUrl.setPath( prifileStr );
        return CameraDiagnostics::NotAuthorisedResult( requestedUrl.toString() );
    }

    if (cam_info_file.length()==0)
    {
        QUrl requestedUrl;
        requestedUrl.setHost( res->getHostAddress() );
        requestedUrl.setPort( 80 );
        requestedUrl.setScheme( QLatin1String("http") );
        requestedUrl.setPath( prifileStr );
        return CameraDiagnostics::NoMediaTrackResult( requestedUrl.toString() );
    }

    if (!isCameraControlDisabled()) {
        if (role != Qn::CR_SecondaryLiveVideo && res->getMotionType() != Qn::MT_SoftwareGrid)
        {
            res->setMotionMaskPhysical(0);
        }
    }

    //=====requesting a video

    if (m_h264)
    //if (m_h264) //look_for_this_comment
    {
        QnDlink_cam_info info = res->getCamInfo();
        if (info.videoProfileUrls.size() < 2 && role == Qn::CR_SecondaryLiveVideo)
        {
            qWarning() << "No dualstreaming for DLink camera " << m_resource->getUrl() << ". Ignore second url request";
            return CameraDiagnostics::NoErrorResult();
        }

        const int dlinkProfile = role == Qn::CR_SecondaryLiveVideo ? 2 : 1;
        QString url =  getRTPurl( dlinkProfile );
        if (url.isEmpty())
        {
            qWarning() << "Invalid answer from DLink camera " << m_resource->getUrl() << ". Expecting non empty rtsl url.";
            return CameraDiagnostics::CameraResponseParseErrorResult( m_resource->getUrl(), lit("config/rtspurl.cgi?profileid=%1").arg(dlinkProfile) );
        }

        m_rtpReader.setRequest(url);
        return m_rtpReader.openStream();
    }
    else
    {
        // mpeg4 or jpeg

        res->init(); // after we changed profile some videoprofile url might be changed

        // ok, now lets open a stream
        QnDlink_cam_info info = res->getCamInfo();
        if (info.videoProfileUrls.size() < 2)
        {
            qWarning() << "Invalid answer from DLink camera " << m_resource->getUrl() << ". Expecting video profile URL.";
            return CameraDiagnostics::CameraResponseParseErrorResult( m_resource->getUrl(), QLatin1String("config/stream_info.cgi") );
        }


        QString url = (role == Qn::CR_SecondaryLiveVideo) ? info.videoProfileUrls[2] : info.videoProfileUrls[1];

        if (url.length() > 1 && url.at(0)==QLatin1Char('/'))
            url = url.mid(1);
        NX_LOG(lit("got stream URL %1 for camera %2 for role %3").arg(url).arg(m_resource->getUrl()).arg(getRole()), cl_logINFO);

        mHttpClient = new CLSimpleHTTPClient(res->getHostAddress(), 80, 2000, res->getAuth());
        const CLHttpStatus status = mHttpClient->doGET(url);
        if( status == CL_HTTP_SUCCESS )
            return CameraDiagnostics::NoErrorResult();
        else
            return CameraDiagnostics::RequestFailedResult(url, QLatin1String(nx_http::StatusCode::toString((nx_http::StatusCode::Value)status)));
    }
}

void PlDlinkStreamReader::closeStream()
{
    delete mHttpClient;
    mHttpClient = 0;
    m_rtpReader.closeStream();
}

bool PlDlinkStreamReader::isStreamOpened() const
{
    return (mHttpClient && mHttpClient->isOpened()) || m_rtpReader.isStreamOpened();
}

QnAbstractMediaDataPtr PlDlinkStreamReader::getNextData()
{
    if (!isStreamOpened())
        return QnAbstractMediaDataPtr(0);

    if (needMetaData())
        return getMetaData();




    if (m_h264) //look_for_this_comment
        //return getNextDataMPEG(CODEC_ID_H264);
        return m_rtpReader.getNextData();

    if (m_mpeg4)
        return getNextDataMPEG(CODEC_ID_MPEG4);

    return getNextDataMJPEG();

}

void PlDlinkStreamReader::updateStreamParamsBasedOnQuality()
{
    if (isRunning())
        pleaseReOpen();
}

void PlDlinkStreamReader::updateStreamParamsBasedOnFps()
{
    if (isRunning())
        pleaseReOpen();
}

//=====================================================================================

int scaleInt(int value, int from, int to)
{
    double step = 1.0 / (double) (from-1);
    return int (step * value * (double) (to-1) + 0.5);
}

bool PlDlinkStreamReader::isTextQualities(const QStringList& qualities) const
{
    if (qualities.isEmpty())
        return false;
    QString val = qualities[0];
    for (int i = 0; i < val.length(); ++i)
    {
        bool isDigit = val.at(i) >= L'0' && val.at(i) <= L'9';
        if (!isDigit)
            return true;
    }

    return false;
}

QString PlDlinkStreamReader::getQualityString() const
{
    QnPlDlinkResourcePtr res = getResource().dynamicCast<QnPlDlinkResource>();
    QnDlink_cam_info info = res->getCamInfo();

    if (!info.hasFixedQuality) 
    {
        int q;
        switch (getQuality())
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
        return QString::number(q);
    }
    int qualityIndex = scaleInt((int) getQuality(), Qn::QualityHighest-Qn::QualityLowest+1, info.possibleQualities.size());
    if (isTextQualities(info.possibleQualities))
        qualityIndex = info.possibleQualities.size()-1 - qualityIndex; // index 0 is best quality if quality is text
    return info.possibleQualities[qualityIndex];
}

QString PlDlinkStreamReader::composeVideoProfile() 
{
    QnPlDlinkResourcePtr res = getResource().dynamicCast<QnPlDlinkResource>();
    QnDlink_cam_info info = res->getCamInfo();

    if (!info.inited())
        return QString();

    Qn::ConnectionRole role = getRole();

    int profileNum;
    QSize resolution;

    if (role == Qn::CR_SecondaryLiveVideo)
    {
        profileNum = 2;
        resolution = info.secondaryStreamResolution();
    }
    else
    {
        profileNum = 1;
        //resolution = info.resolutionCloseTo(320);
        resolution = info.primaryStreamResolution();
    }


    QString result;
    QTextStream t(&result);

    t << "config/video.cgi?profileid=" << profileNum;
    if (!isCameraControlDisabled())
    {
        t << "&resolution=" << resolution.width() << "x" << resolution.height() << "&";

        int fps = info.frameRateCloseTo( qMin((int)getFps(), res->getMaxFps()) );
        t << "framerate=" << fps << "&";
        t << "codec=";


        
        m_mpeg4 = false;
        m_h264 = false;

        //bool hasSdp = false; 
        bool hasSdp = info.videoProfileUrls.contains(3) && info.videoProfileUrls[4].toLower().contains(QLatin1String("sdp")); //look_for_this_comment

        if (!info.hasH264.isEmpty() && !hasSdp)
        {
            t << info.hasH264 << "&";
            m_h264 = true;
            
        }
        else if (info.hasMPEG4 && !hasSdp)
        {
            t << "MPEG4&";
            m_mpeg4 = true;
        }
        else
        {
            t << "MJPEG&";
        }

        if (m_mpeg4 || m_h264)
        {
            // just CBR fo mpeg so far
            t << "qualitymode=CBR" << "&";
            t << "bitrate=" <<  info.bitrateCloseTo(res->suggestBitrateKbps(getQuality(), resolution, fps));
        }
        else
        {
            t << "qualitymode=Fixquality" << "&";
            
            t << "quality=" << getQualityString();
        }
    }

    t.flush();
    return result;
}

QString PlDlinkStreamReader::getRTPurl(int profileId) const
{
    CLHttpStatus status;

    QnPlDlinkResourcePtr res = getResource().dynamicCast<QnPlDlinkResource>();

    QByteArray reply = downloadFile(status, QString(lit("config/rtspurl.cgi?profileid=%1")).arg(profileId),  res->getHostAddress(), 80, 1000, res->getAuth());

    if (status != CL_HTTP_SUCCESS || reply.isEmpty())
        return QString();

    int lastProfileId = -1;
    QByteArray requiredProfile = QString(lit("profileid=%1")).arg(profileId).toUtf8();
    QList<QByteArray> lines = reply.split('\n');
    foreach(QByteArray line, lines)
    {
        line = line.toLower().trimmed();
        if (line.startsWith("profileid="))
            lastProfileId = line.split('=')[1].toInt();
        else if (line.startsWith("urlentry=") && lastProfileId == profileId)
            return QString::fromUtf8(line.split('=')[1]);
    }

    return QString();
}

QnAbstractMediaDataPtr PlDlinkStreamReader::getNextDataMPEG(CodecID ci)
{
    char headerBuffer[sizeof(ACS_VideoHeader)];
    uint gotInBuffer = 0;
    while (gotInBuffer < sizeof(ACS_VideoHeader))
    {
        int readed = mHttpClient->read(headerBuffer + gotInBuffer, sizeof(ACS_VideoHeader) - gotInBuffer);

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
        int readed = mHttpClient->read(curPtr, dataLeft);
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
        int readed = mHttpClient->read(headerBuffer+headerSize, sizeof(headerBuffer)-1 - headerSize);
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

    int dataLeft = contentLen - videoData->dataSize();
    char* curPtr = videoData->m_data.data() + videoData->m_data.size();
    videoData->m_data.finishWriting(dataLeft);

    while (dataLeft > 0)
    {
        int readed = mHttpClient->read(curPtr, dataLeft);
        if (readed < 1)
            return QnAbstractMediaDataPtr(0);
        curPtr += readed;
        dataLeft -= readed;
    }
    // sometime 1 more bytes in the buffer end. Looks like it is a DLink bug caused by 16-bit word alignment
    if (contentLen > 2 && !(curPtr[-2] == (char)0xff && curPtr[-1] == (char)0xd9))
        videoData->m_data.finishWriting(-1);

    videoData->compressionType = CODEC_ID_MJPEG;
    videoData->width = 1920;
    videoData->height = 1088;
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

    foreach(QString line, lines)
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

#endif // #ifdef ENABLE_DLINK
