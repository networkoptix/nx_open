#include "droid_stream_reader.h"
#include "droid_resource.h"


PlDroidStreamReader::PlDroidStreamReader(QnResourcePtr res):
CLServerPushStreamreader(res),
m_sock(0)
{

}

PlDroidStreamReader::~PlDroidStreamReader()
{

}

QnAbstractMediaDataPtr PlDroidStreamReader::getNextData()
{
    if (!isStreamOpened())
        return QnAbstractMediaDataPtr(0);

    char buff[1460];

    FILE* f = fopen("c:/droid.mp4", "wb");
    
    


    while (1)
    {
        int readed = m_sock->recv(buff, 1460);

        fwrite(buff, readed, 1, f);

        if (readed < 1)
        {
            fclose(f);
            return QnAbstractMediaDataPtr(0);
        }
    }

    fclose(f);

    /*

    quint16 chunkSize = 0;
    int readed = m_sock->recv((char*)&chunkSize, 2);
    if (readed < 2)
        return QnAbstractMediaDataPtr(0);


    QnCompressedVideoDataPtr videoData(new QnCompressedVideoData(CL_MEDIA_ALIGNMENT, chunkSize+FF_INPUT_BUFFER_PADDING_SIZE));
    char* curPtr = videoData->data.data();
    videoData->data.prepareToWrite(chunkSize); // this call does nothing 
    videoData->data.done(chunkSize);

    int dataLeft = chunkSize;

    while (dataLeft > 0)
    {
        int readed = m_sock->recv(curPtr, dataLeft);
        if (readed < 1)
            return QnAbstractMediaDataPtr(0);
        curPtr += readed;
        dataLeft -= readed;
    }

    FILE* f = fopen("c:/droid.mp4", "ab");
    fwrite(videoData->data.data(), chunkSize, 1, f);
    fclose(f);


    videoData->compressionType = CODEC_ID_H264;
    videoData->width = 320;
    videoData->height = 240;

    
    videoData->flags |= AV_PKT_FLAG_KEY;

    videoData->channelNumber = 0;
    videoData->timestamp = qnSyncTime->currentMSecsSinceEpoch() * 1000;

    return videoData;

    /**/

}

void PlDroidStreamReader::openStream()
{
    if (isStreamOpened())
        return;
    
    QnDroidResourcePtr res = getResource().dynamicCast<QnDroidResource>();

    m_sock = new TCPSocket();
    m_sock->setReadTimeOut(20000);
    m_sock->setWriteTimeOut(20000);

    if (!m_sock->connect(res->getHostAddress().toString().toLatin1().data(), res->getVideoPort()))
        closeStream();

    
}

void PlDroidStreamReader::closeStream()
{
    if (m_sock)
    {
        m_sock->close();
        delete m_sock;
        m_sock = 0;
    }
}

bool PlDroidStreamReader::isStreamOpened() const
{
    return ( m_sock && m_sock->isConnected() );
}

void PlDroidStreamReader::updateStreamParamsBasedOnQuality()
{

}

void PlDroidStreamReader::updateStreamParamsBasedOnFps()
{

}
