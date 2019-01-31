#include "mjpeg_stream_reader.h"
#if defined(ENABLE_DATA_PROVIDERS)

#include "nx/streaming/video_data_packet.h"

#include <nx/vms/server/resource/camera.h>

#include "utils/common/synctime.h"
#include <nx/network/http/http_types.h>
#include "utils/media/jpeg_utils.h"

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

MJPEGStreamReader::MJPEGStreamReader(
    const nx::vms::server::resource::CameraPtr& res,
    const QString& streamHttpPath)
:
    CLServerPushStreamReader(res),
    m_request(streamHttpPath),
    m_camera(res)
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

    auto allocSize = contentLen + FF_INPUT_BUFFER_PADDING_SIZE;
    if (allocSize > MAX_ALLOWED_FRAME_SIZE)
        return QnWritableCompressedVideoDataPtr();

    QnWritableCompressedVideoDataPtr videoData(new QnWritableCompressedVideoData(CL_MEDIA_ALIGNMENT, allocSize));
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

    videoData->compressionType = AV_CODEC_ID_MJPEG;

    nx_jpg::ImageInfo imgInfo;
    if( nx_jpg::readJpegImageInfo( (const quint8*)videoData->data(), videoData->dataSize(), &imgInfo ) )
    {
        videoData->width = imgInfo.width;
        videoData->height = imgInfo.height;
    }
    videoData->flags |= QnAbstractMediaData::MediaFlags_AVKey;
    videoData->channelNumber = 0;
    videoData->timestamp = qnSyncTime->currentMSecsSinceEpoch() * 1000;

    return videoData;
}

CameraDiagnostics::Result MJPEGStreamReader::openStreamInternal(bool /*isCameraControlRequired*/, const QnLiveStreamParams& /*params*/)
{
    if (isStreamOpened())
        return CameraDiagnostics::NoErrorResult();

    //QString request = QLatin1String("now.jpg?snap=spush?dummy=1305868336917");

    mHttpClient.reset(
        new CLSimpleHTTPClient(
            m_camera->getHostAddress(),
            m_camera->httpPort(),
            2000,
            m_camera->getAuth()));
    CLHttpStatus httpStatus = mHttpClient->doGET(m_request);

	QUrl requestedUrl;
	requestedUrl.setHost(m_camera->getHostAddress());
	requestedUrl.setPort(m_camera->httpPort());
	requestedUrl.setScheme(QLatin1String("http"));
	requestedUrl.setPath(m_request);

    m_camera->updateSourceUrl(requestedUrl.toString(), getRole());

    switch( httpStatus )
    {
        case CL_HTTP_SUCCESS:
            return CameraDiagnostics::NoErrorResult();
        case CL_HTTP_AUTH_REQUIRED:
            return CameraDiagnostics::NotAuthorisedResult( requestedUrl.toString() );
        default:
            return CameraDiagnostics::RequestFailedResult(m_request, QLatin1String(nx::network::http::StatusCode::toString((nx::network::http::StatusCode::Value)httpStatus)));
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

#endif // defined(ENABLE_DATA_PROVIDERS)
