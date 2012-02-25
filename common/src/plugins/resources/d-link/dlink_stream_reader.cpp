#include "dlink_stream_reader.h"
#include <QTextStream>
#include "dlink_resource.h"

extern int bestBitrateKbps(QnStreamQuality q, QSize resolution, int fps);
extern inline int getIntParam(const char* pos);

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
mHttpClient(0),
m_h264(false),
m_mpeg4(false)
{
}

PlDlinkStreamReader::~PlDlinkStreamReader()
{

}


void PlDlinkStreamReader::openStream()
{
    if (isStreamOpened())
        return;

    //setRole(QnResource::Role_SecondaryLiveVideo);

    QnPlDlinkResourcePtr res = getResource().dynamicCast<QnPlDlinkResource>();

    QString prifileStr = composeVideoProfile();

    if (prifileStr.length()==0)
        return;

    CLHttpStatus status;
    QByteArray cam_info_file = downloadFile(status, prifileStr,  res->getHostAddress(), 80, 1000, res->getAuth());

    if (status == CL_HTTP_AUTH_REQUIRED)
    {
        getResource()->setStatus(QnResource::Unauthorized);
        return;
    }


    if (cam_info_file.length()==0)
        return;

    res->init(); // after we changed profile some videoprofile url might be changed
    
    // ok, now lets open a stream

    QnDlink_cam_info info = res->getCamInfo();
    if (info.videoProfileUrls.size() < 2)
    {
        Q_ASSERT(false);
        return;
    }


    QString url = (getRole() == QnResource::Role_SecondaryLiveVideo) ? info.videoProfileUrls[2] : info.videoProfileUrls[1];

    if (url.length() > 1 && url.at(0)=='/')
        url = url.mid(1);

    mHttpClient = new CLSimpleHTTPClient(res->getHostAddress(), 80, 2000, res->getAuth());
    mHttpClient->doGET(url);
}

void PlDlinkStreamReader::closeStream()
{
    delete mHttpClient;
    mHttpClient = 0;
}

bool PlDlinkStreamReader::isStreamOpened() const
{
    return ( mHttpClient && mHttpClient->isOpened() );
}

QnAbstractMediaDataPtr PlDlinkStreamReader::getNextData()
{
    if (!isStreamOpened())
        return QnAbstractMediaDataPtr(0);

    if (needMetaData())
        return getMetaData();


    if (m_h264)
        return getNextDataMPEG(CODEC_ID_H264);

    if (m_mpeg4)
        return getNextDataMPEG(CODEC_ID_MPEG4);

    return getNextDataMJPEG();

}

void PlDlinkStreamReader::updateStreamParamsBasedOnQuality()
{
    bool runing = isRunning();
    pleaseStop();
    stop();
    closeStream();
    
    if (runing)
    {
        start(); // it will call openstream by itself
    }

}

void PlDlinkStreamReader::updateStreamParamsBasedOnFps()
{
    bool runing = isRunning();
    pleaseStop();
    stop();
    closeStream();

    if (runing)
    {
        start(); // it will call openstream by itself
    }

}

//=====================================================================================

QString PlDlinkStreamReader::composeVideoProfile() 
{
    QnPlDlinkResourcePtr res = getResource().dynamicCast<QnPlDlinkResource>();
    QnDlink_cam_info info = res->getCamInfo();

    if (!info.inited())
        return "";

    QnResource::ConnectionRole role = getRole();

    int profileNum;
    QSize resolution;

    if (role == QnResource::Role_SecondaryLiveVideo)
    {
        profileNum = 2;
        resolution = info.resolutionCloseTo(320);
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

    bool hasSdp = info.videoProfileUrls.contains(3) && info.videoProfileUrls[4].toLower().contains("sdp");

    if (info.hasH264!="" && !hasSdp)
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
        t << "bitrate=" <<  info.bitrateCloseTo(bestBitrateKbps(getQuality(), resolution, fps));
    }
    else
    {
        t << "qualitymode=Fixquality" << "&";

        int q = 60;
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
        }

        t << "quality=" << 1;
    }

    t.flush();
    return result;
}

QnAbstractMediaDataPtr PlDlinkStreamReader::getNextDataMPEG(CodecID ci)
{
    char headerBuffer[sizeof(ACS_VideoHeader)];
    int gotInBuffer = 0;
    while (gotInBuffer < sizeof(ACS_VideoHeader))
    {
        int readed = mHttpClient->read(headerBuffer + gotInBuffer, sizeof(ACS_VideoHeader) - gotInBuffer);

        if (readed < 1)
            return QnAbstractMediaDataPtr(0);

        gotInBuffer += readed;
    }

    ACS_VideoHeader *vh = (ACS_VideoHeader*)(headerBuffer);

    int dataLeft = qMin(vh->ulDataLength, (quint32)1024*1024*10); // to avoid crash


    QnCompressedVideoDataPtr videoData(new QnCompressedVideoData(CL_MEDIA_ALIGNMENT, dataLeft+FF_INPUT_BUFFER_PADDING_SIZE));
    char* curPtr = videoData->data.data();
    videoData->data.prepareToWrite(dataLeft); // this call does nothing 
    videoData->data.done(dataLeft);

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
    videoData->timestamp = QDateTime::currentMSecsSinceEpoch() * 1000;

    return videoData;

}


QnAbstractMediaDataPtr PlDlinkStreamReader::getNextDataMJPEG()
{
    char headerBuffer[512+1];
    int headerSize = 0;
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

    QnCompressedVideoDataPtr videoData(new QnCompressedVideoData(CL_MEDIA_ALIGNMENT, contentLen+FF_INPUT_BUFFER_PADDING_SIZE));
    videoData->data.write(realHeaderEnd+4, headerBufferEnd - (realHeaderEnd+4));

    int dataLeft = contentLen - videoData->data.size();
    char* curPtr = videoData->data.data() + videoData->data.size();
    videoData->data.done(dataLeft);

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
        videoData->data.done(-1);

    videoData->compressionType = CODEC_ID_MJPEG;
    videoData->width = 1920;
    videoData->height = 1088;
    videoData->flags |= AV_PKT_FLAG_KEY;
    videoData->channelNumber = 0;
    videoData->timestamp = QDateTime::currentMSecsSinceEpoch() * 1000;

    return videoData;

}


QnMetaDataV1Ptr PlDlinkStreamReader::getMetaData()
{

    QnPlDlinkResourcePtr res = getResource().dynamicCast<QnPlDlinkResource>();


    CLHttpStatus status;
    QByteArray cam_info_file = downloadFile(status, "config/notify.cgi",  res->getHostAddress(), 80, 1000, res->getAuth());

    if (status == CL_HTTP_AUTH_REQUIRED)
    {
        res->setStatus(QnResource::Unauthorized);
        return QnMetaDataV1Ptr(0);
    }

    if (cam_info_file.size()==0)
        return QnMetaDataV1Ptr(0);

    QString file_s(cam_info_file);
    QStringList lines = file_s.split("\r\n", QString::SkipEmptyParts);

    QnMetaDataV1Ptr motion(new QnMetaDataV1());

    foreach(QString line, lines)
    {
        QString line_low = line.toLower();

        if (line_low.contains("md0") || line_low.contains("md1") || line_low.contains("md2"))
        {

            for (int x = 0; x < MD_WIDTH; ++x)
            {
                for (int y = 0; y < MD_HEIGHT; ++y)
                {
                    motion->setMotionAt(x,y);
                }
            }


        }
    }


    

    //motion->m_duration = META_DATA_DURATION_MS * 1000 ;
    motion->m_duration = 1000*1000*1000; // 1000 sec 
    return motion;
}
