#include "dlink_stream_reader.h"
#include <QTextStream>
#include "dlink_resource.h"
#include "utils/common/sleep.h"
#include "utils/common/synctime.h"



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

PlDlinkStreamReader::PlDlinkStreamReader(QnResourcePtr res):
CLServerPushStreamreader(res),
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


void PlDlinkStreamReader::openStream()
{
    if (isStreamOpened())
        return;

    //setRole(QnResource::Role_SecondaryLiveVideo);

    //==== init if needed
    QnResource::ConnectionRole role = getRole();
    QnPlDlinkResourcePtr res = getResource().dynamicCast<QnPlDlinkResource>();
    if (!res->getCamInfo().inited())
    {
        res->init();
    }

    //==== profile setup
    QString prifileStr = composeVideoProfile();

    if (prifileStr.length()==0)
        return;

    CLHttpStatus status;
    QByteArray cam_info_file = downloadFile(status, prifileStr,  res->getHostAddress(), 80, 1000, res->getAuth()); // setup video profile

    if (status == CL_HTTP_AUTH_REQUIRED)
    {
        getResource()->setStatus(QnResource::Unauthorized);
        return;
    }

    if (cam_info_file.length()==0)
        return;

    if (role != QnResource::Role_SecondaryLiveVideo && res->getMotionType() != Qn::MT_SoftwareGrid)
    {
        res->setMotionMaskPhysical(0);
    }

    //=====requesting a video

    if (m_h264)
    //if (m_h264) //look_for_this_comment
    {
        QnDlink_cam_info info = res->getCamInfo();
        if (info.videoProfileUrls.size() < 2 && role == QnResource::Role_SecondaryLiveVideo)
        {
            qWarning() << "No dualstreaming for DLink camera " << m_resource->getUrl() << ". Ignore second url request";
            return;
        }

        QString url =  getRTPurl(role == QnResource::Role_SecondaryLiveVideo ? 2 : 1);
        if (url.isEmpty())
        {
            qWarning() << "Invalid answer from DLink camera " << m_resource->getUrl() << ". Expecting non empty rtsl url.";
            return;
        }

        m_rtpReader.setRequest(url);
        m_rtpReader.openStream();
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
            return;
        }


        QString url = (role == QnResource::Role_SecondaryLiveVideo) ? info.videoProfileUrls[2] : info.videoProfileUrls[1];

        if (url.length() > 1 && url.at(0)==QLatin1Char('/'))
            url = url.mid(1);

        mHttpClient = new CLSimpleHTTPClient(res->getHostAddress(), 80, 2000, res->getAuth());
        mHttpClient->doGET(url);

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
    return int (((double) value / (double) from) * to + 0.5);
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
        case QnQualityHighest:
            q = 90;
            break;

        case QnQualityHigh:
            q = 80;
            break;

        case QnQualityNormal:
            q = 70;
            break;

        case QnQualityLow:
            q = 50;
            break;

        case QnQualityLowest:
            q = 40;
            break;

        default:
            q = 60;
            break;
        }
        return QString::number(q);
    }
    int qualityIndex = scaleInt((int) getQuality(), QnQualityHighest-QnQualityLowest+1, info.possibleQualities.size());
    return info.possibleQualities[info.possibleQualities.size()-1 - qualityIndex]; // 0 is best quality for dlink
}

QString PlDlinkStreamReader::composeVideoProfile() 
{
    QnPlDlinkResourcePtr res = getResource().dynamicCast<QnPlDlinkResource>();
    QnDlink_cam_info info = res->getCamInfo();

    if (!info.inited())
        return QString();

    QnResource::ConnectionRole role = getRole();

    int profileNum;
    QSize resolution;

    if (role == QnResource::Role_SecondaryLiveVideo)
    {
        profileNum = 2;
        resolution = info.resolutionCloseTo(480);
    }
    else
    {
        profileNum = 1;
        //resolution = info.resolutionCloseTo(320);
        resolution = info.resolutions.at(0);
    }


    QString result;
    QTextStream t(&result);

    t << "config/video.cgi?profileid=" << profileNum << "&";
    t << "resolution=" << resolution.width() << "x" << resolution.height() << "&";

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


    QnCompressedVideoDataPtr videoData(new QnCompressedVideoData(CL_MEDIA_ALIGNMENT, dataLeft+FF_INPUT_BUFFER_PADDING_SIZE));
    char* curPtr = videoData->data.data();
    videoData->data.startWriting(dataLeft); // this call does nothing 
    videoData->data.finishWriting(dataLeft);

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
        videoData->flags |= AV_PKT_FLAG_KEY;

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

    QnCompressedVideoDataPtr videoData(new QnCompressedVideoData(CL_MEDIA_ALIGNMENT, contentLen+FF_INPUT_BUFFER_PADDING_SIZE));
    videoData->data.write(realHeaderEnd+4, headerBufferEnd - (realHeaderEnd+4));

    int dataLeft = contentLen - videoData->data.size();
    char* curPtr = videoData->data.data() + videoData->data.size();
    videoData->data.finishWriting(dataLeft);

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
        videoData->data.finishWriting(-1);

    videoData->compressionType = CODEC_ID_MJPEG;
    videoData->width = 1920;
    videoData->height = 1088;
    videoData->flags |= AV_PKT_FLAG_KEY;
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
        res->setStatus(QnResource::Unauthorized);
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
    return motion;
}
