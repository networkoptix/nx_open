#include "onvif_mjpeg.h"

#ifdef ENABLE_DATA_PROVIDERS

#include "core/datapacket/video_data_packet.h"
#include "core/resource/network_resource.h"
#include "utils/common/synctime.h"
#include "utils/network/http/httptypes.h"

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

MJPEGStreamReader::MJPEGStreamReader(const QnResourcePtr& res, const QString& streamHttpPath)
:
    CLServerPushStreamReader(res),
    m_request(streamHttpPath)
{

}

MJPEGStreamReader::~MJPEGStreamReader()
{
    stop();
}

int getIntParam(const char* pos)
{
    int rez = 0;
    for (; *pos >= '0' && *pos <= '9'; pos++)
        rez = rez*10 + (*pos-'0');
    return rez;
}

QnAbstractMediaDataPtr MJPEGStreamReader::getNextData()
{
    if (!isStreamOpened())
        return QnAbstractMediaDataPtr(0);

    char headerBuffer[512+1];
    uint headerSize = 0;
    char* headerBufferEnd = 0;
    char* realHeaderEnd = 0;
    int readed;
    while (headerSize < sizeof(headerBuffer)-1)
    {
        readed = mHttpClient->read(headerBuffer+headerSize, sizeof(headerBuffer)-1 - headerSize);
        if (readed < 1)
            return QnAbstractMediaDataPtr(0);
        headerSize += readed;
        headerBufferEnd = headerBuffer + headerSize;
        *headerBufferEnd = 0;
        realHeaderEnd = strstr(headerBuffer + 1, "\r\n\r\n");
        if (realHeaderEnd)
            break;
    }
    if (!realHeaderEnd)
        return QnAbstractMediaDataPtr(0);
    char* contentLenPtr = strstr(headerBuffer, "Content-Length:");
    if (!contentLenPtr)
        return QnAbstractMediaDataPtr(0);
    int contentLen = getIntParam(contentLenPtr + 16);

    QnWritableCompressedVideoDataPtr videoData(new QnWritableCompressedVideoData(CL_MEDIA_ALIGNMENT, contentLen+FF_INPUT_BUFFER_PADDING_SIZE));
    videoData->m_data.write(realHeaderEnd+4, headerBufferEnd - (realHeaderEnd+4));

    int dataLeft = contentLen - videoData->m_data.size();
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

CameraDiagnostics::Result MJPEGStreamReader::openStream()
{
    if (isStreamOpened())
        return CameraDiagnostics::NoErrorResult();

    //QString request = QLatin1String("now.jpg?snap=spush?dummy=1305868336917");
    QnNetworkResourcePtr nres = getResource().dynamicCast<QnNetworkResource>();

    mHttpClient.reset( new CLSimpleHTTPClient(nres->getHostAddress(), nres->httpPort() , 2000, nres->getAuth()) );
    CLHttpStatus httpStatus = mHttpClient->doGET(m_request);
    switch( httpStatus )
    {
        case CL_HTTP_SUCCESS:
            return CameraDiagnostics::NoErrorResult();
        case CL_HTTP_AUTH_REQUIRED:
        {
            QUrl requestedUrl;
            requestedUrl.setHost( nres->getHostAddress() );
            requestedUrl.setPort( nres->httpPort() );
            requestedUrl.setScheme( QLatin1String("http") );
            requestedUrl.setPath( m_request );
            return CameraDiagnostics::NotAuthorisedResult( requestedUrl.toString() );
        }
        default:
            return CameraDiagnostics::RequestFailedResult(m_request, QLatin1String(nx_http::StatusCode::toString((nx_http::StatusCode::Value)httpStatus)));
    }
}

void MJPEGStreamReader::closeStream()
{
    mHttpClient.reset();
}

bool MJPEGStreamReader::isStreamOpened() const
{
    return ( mHttpClient.get() && mHttpClient->isOpened() );
}

#endif // ENABLE_DATA_PROVIDERS
