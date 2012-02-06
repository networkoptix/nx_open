#include "onvif_mjpeg.h"
#include "core/resource/network_resource.h"

/*
inline static int findJPegStartCode(const char *data, int datalen)
{
    const char* end = data + datalen-1;
    for(const char* curPtr = data; curPtr < end; ++curPtr)
    {
        if (*curPtr == (char)0xff && curPtr[1] == (char)0xd8)
            return curPtr - data;
    }
    return -1;
}

inline static int findJPegEndCode(const char *data, int datalen)
{
    const char* end = data + datalen-1;
    for(const char* curPtr = data; curPtr < end; ++curPtr)
    {
        if (*curPtr == (char)0xff && curPtr[1] == (char)0xd9)
            return curPtr - data;
    }
    return -1;
}
*/

/*
char jpeg_start[2] = {0xff, 0xd8};
char jpeg_end[2] = {0xff, 0xd9};

int contain_subst(char *data, int datalen, char *subdata, int subdatalen)
{
    if (!data || !subdata || datalen<=0 || subdatalen <= 0 )
        return -1;

    int coincidence_len = 0;
    char *pdata = data;


    while(1)
    {
        if (*pdata == subdata[coincidence_len])
            ++coincidence_len;
        else
            coincidence_len = 0;

        if (coincidence_len==subdatalen)
            return (pdata-data - subdatalen + 1);

        if (pdata-data==datalen-1)
            return -1;//not found

        pdata++;

    }
}
*/

MJPEGtreamreader::MJPEGtreamreader(QnResourcePtr res, const QString& requst)
:CLServerPushStreamreader(res),
mHttpClient(0),
m_request(requst)
{

}

MJPEGtreamreader::~MJPEGtreamreader()
{
    stop();
}

inline int getIntParam(const char* pos)
{
    int rez = 0;
    for (; *pos >= '0' && *pos <= '9'; pos++)
        rez = rez*10 + (*pos-'0');
    return rez;
}

QnAbstractMediaDataPtr MJPEGtreamreader::getNextData()
{
    if (!isStreamOpened())
        return QnAbstractMediaDataPtr(0);

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

void MJPEGtreamreader::openStream()
{
    if (isStreamOpened())
        return;

    //QString request = QLatin1String("now.jpg?snap=spush?dummy=1305868336917");
    QnNetworkResourcePtr nres = getResource().dynamicCast<QnNetworkResource>();

    mHttpClient = new CLSimpleHTTPClient(nres->getHostAddress(), 80, 2000, nres->getAuth());
    mHttpClient->doGET(m_request);
}

void MJPEGtreamreader::closeStream()
{
    delete mHttpClient;
    mHttpClient = 0;
}

bool MJPEGtreamreader::isStreamOpened() const
{
    return ( mHttpClient && mHttpClient->isOpened() );
}
