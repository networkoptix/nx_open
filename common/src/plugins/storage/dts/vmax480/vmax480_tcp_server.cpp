#include "vmax480_tcp_server.h"
#include "utils/network/tcp_connection_priv.h"
#include <QUuid>
#include "vmax480_helper.h"
#include "vmax480_live_reader.h"

static const int CONNECTION_TIMEOUT = 3 * 1000 * 1000;
static const int UUID_LEN = 38;

enum V_CODEC_TYPE{ CODEC_VSTREAM_H264 , CODEC_VSTREAM_JPEG , CODEC_VSTREAM_MPEG4};
enum A_CODEC_TYPE{ CODEC_ASTREAM_MULAW , CODEC_ASTREAM_G723, CODEC_ASTREAM_IMAACPCM, CODEC_ASTREAM_MSADPCM, CODEC_ASTREAM_PCM, CODEC_ASTREAM_CG726, CODEC_ASTREAM_CG711A, CODEC_ASTREAM_CG711U};


Q_GLOBAL_STATIC(QnVMax480Server, inst)

QnVMax480Server* QnVMax480Server::instance()
{
    return inst();
}

QnVMax480Server::QnVMax480Server():
    QnTcpListener(QHostAddress(QLatin1String("127.0.0.1")), 0)
{
    start();
}

QnVMax480Server::~QnVMax480Server()
{

}

QnTCPConnectionProcessor* QnVMax480Server::createRequestProcessor(TCPSocket* clientSocket, QnTcpListener* owner)
{
    return new QnVMax480ConnectionProcessor(clientSocket, owner);
}

QString QnVMax480Server::registerProvider(QnVMax480LiveProvider* provider)
{
    QString id = QUuid::createUuid();

    QMutexLocker lock(&m_providersMutex);
    m_providers.insert(id, provider);
    return id;
}

void QnVMax480Server::unregisterProvider(QnVMax480LiveProvider* provider)
{
    QMutexLocker lock(&m_providersMutex);
    for (QMap<QString, QnVMax480LiveProvider*>::iterator itr = m_providers.begin(); itr != m_providers.end(); ++itr)
    {
        if (itr.value() == provider) {
            m_providers.erase(itr);
            break;
        }
    }
}

void QnVMax480Server::registerConnectionProcessor(const QString& tcpID, QnVMax480ConnectionProcessor* processor)
{
    QMutexLocker lock(&m_providersMutex);
    m_processors.insert(tcpID, processor);
}

void QnVMax480Server::unregisterConnectionProcessor(const QString& tcpID)
{
    QMutexLocker lock(&m_providersMutex);
    m_processors.remove(tcpID);
}

QnVMax480ConnectionProcessor* QnVMax480Server::waitForConnection(const QString& tcpID, int timeout)
{
    QTime t;
    t.restart();
    QnVMax480ConnectionProcessor* result = 0;

    QMutexLocker lock(&m_providersMutex);
    do {
        result = m_processors.value(tcpID);
        if (!result)
            msleep(5);
    } while (result == 0 && t.elapsed() < timeout);
    
    return result;
}

void QnVMax480Server::onGotData(const QString& tcpID, QnAbstractMediaDataPtr mediaData)
{
    QMutexLocker lock(&m_providersMutex);
    QnVMax480LiveProvider* provider = m_providers.value(tcpID);
    if (provider)
        provider->onGotData(mediaData);
}

// -------------- QnVMax480Consumer -------------------

class QnVMax480ConnectionProcessorPrivate: public QnTCPConnectionProcessorPrivate
{
public:
    QnVMax480ConnectionProcessorPrivate(): QnTCPConnectionProcessorPrivate(), sequence(0) {}
public:
    QString tcpID;
    quint8 sequence;
    QnMediaContextPtr context;
};

QnVMax480ConnectionProcessor::QnVMax480ConnectionProcessor(TCPSocket* socket, QnTcpListener* _owner):
    QnTCPConnectionProcessor(new QnVMax480ConnectionProcessorPrivate, socket, _owner)
{
    Q_D(QnVMax480ConnectionProcessor);
}

QnVMax480ConnectionProcessor::~QnVMax480ConnectionProcessor()
{
    Q_D(QnVMax480ConnectionProcessor);
    stop();
    static_cast<QnVMax480Server*>(d->owner)->unregisterConnectionProcessor(d->tcpID);
}

void QnVMax480ConnectionProcessor::run()
{
    Q_D(QnVMax480ConnectionProcessor);

    d->socket->setReadTimeOut(CONNECTION_TIMEOUT);
    d->socket->setWriteTimeOut(CONNECTION_TIMEOUT);

    char guidBuffer[UUID_LEN+1];
    int readed = d->socket->recv(guidBuffer, UUID_LEN);
    if (readed == UUID_LEN)
    {
        guidBuffer[UUID_LEN] = 0;
        d->tcpID = QString::fromLocal8Bit(guidBuffer);
        static_cast<QnVMax480Server*>(d->owner)->registerConnectionProcessor(d->tcpID, this);
    }

    while (!m_needStop)
    {
        QnAbstractMediaData* mediaData = 0;
        quint8 packetHeader[16];
        int readed = d->socket->recv(packetHeader, sizeof(packetHeader));
        if (readed == sizeof(packetHeader))
        {
            quint8 sequence = packetHeader[0];
            quint8 codec = packetHeader[2];
            quint32 dataSize = *(quint32*)(packetHeader+4);
            quint64 timestamp = *(quint64*)(packetHeader+8);

            if ((VMaxDataType) packetHeader[1] == VMAXDT_GotVideoPacket) 
            {

                bool isIFrame = packetHeader[3];

                QnCompressedVideoData* videoData = new QnCompressedVideoData(CL_MEDIA_ALIGNMENT, dataSize);
                mediaData = videoData;
                if (isIFrame) 
                    videoData->flags |= AV_PKT_FLAG_KEY;
                
                CodecID compressionType = CODEC_ID_NONE;
                switch (codec) {
                    case CODEC_VSTREAM_H264:
                        compressionType = CODEC_ID_H264;
                        break;
                    case CODEC_VSTREAM_JPEG:
                        compressionType = CODEC_ID_MJPEG;
                        break;
                    case CODEC_VSTREAM_MPEG4:
                        compressionType = CODEC_ID_MPEG4;
                        break;
                }
                videoData->compressionType = compressionType;

            }
            else if ((VMaxDataType) packetHeader[1] == VMAXDT_GotAudioPacket) 
            {
                quint8 VMaxAudioHeader[4];

                readed = d->socket->recv(VMaxAudioHeader, sizeof(VMaxAudioHeader));
                if (readed != sizeof(VMaxAudioHeader))
                    break;

                CodecID compressionType = CODEC_ID_NONE;
                AVSampleFormat sampleFormat = AV_SAMPLE_FMT_U8;
                switch (codec) {
                    CODEC_ASTREAM_MULAW: 
                        compressionType = CODEC_ID_PCM_MULAW;
                        break;
                    CODEC_ASTREAM_G723:
                        compressionType = CODEC_ID_G723_1;
                        break;
                    CODEC_ASTREAM_IMAACPCM:
                        compressionType = CODEC_ID_ADPCM_IMA_WAV;
                        break;
                    CODEC_ASTREAM_MSADPCM:
                        compressionType = CODEC_ID_ADPCM_MS;
                        break;
                    CODEC_ASTREAM_PCM:
                        compressionType = CODEC_ID_PCM_S8;
                        break;
                    CODEC_ASTREAM_CG726:
                        compressionType = CODEC_ID_ADPCM_G726;
                        sampleFormat = AV_SAMPLE_FMT_S16;
                        break;
                    CODEC_ASTREAM_CG711A:
                        compressionType = CODEC_ID_PCM_ALAW;
                        break;
                    CODEC_ASTREAM_CG711U:
                        compressionType = CODEC_ID_PCM_MULAW;
                        break;
                }

                if (!d->context) {
                    d->context = QnMediaContextPtr(new QnMediaContext(compressionType));
                    d->context->ctx()->channels = VMaxAudioHeader[0];
                    d->context->ctx()->bits_per_coded_sample = VMaxAudioHeader[1];
                    d->context->ctx()->sample_rate = *(quint16*)(VMaxAudioHeader+2);
                    d->context->ctx()->sample_fmt = sampleFormat;
                    d->context->ctx()->time_base.num = 1;
                    d->context->ctx()->time_base.den = d->context->ctx()->sample_rate;
                }
                QnCompressedAudioData* audioData = new QnCompressedAudioData(CL_MEDIA_ALIGNMENT, dataSize, d->context);
                audioData->compressionType = compressionType;
                mediaData = audioData;
            }
            else {
                break; // error
            }

            if (mediaData) {
                int toRead = dataSize;
                QnByteArray& data = mediaData->data;
                while (toRead > 0 && !m_needStop) {
                    int readed = d->socket->recv(data.data() + data.size(), toRead);
                    if (readed < 1)
                        break; // error
                    data.resize(data.size() + readed);
                    toRead -= readed;
                }

                if (toRead == 0) {
                    mediaData->timestamp = timestamp;
                    static_cast<QnVMax480Server*>(d->owner)->onGotData(d->tcpID, QnAbstractMediaDataPtr(mediaData));
                }
            }
        }
        else {
            break; // error
        }
    }

    d->socket->close();
}

void QnVMax480ConnectionProcessor::connect(const QUrl& url, const QAuthenticator& auth)
{
    Q_D(QnVMax480ConnectionProcessor);
    VMaxParamList params;
    params["url"] = url.toString().toUtf8();
    params["login"] = auth.user().toUtf8();
    params["password"] = auth.password().toUtf8();

    // todo: add archive/live switching here
    d->socket->send(QnVMax480Helper::serializeCommand(Command_OpenLive, d->sequence, params));
}

void QnVMax480ConnectionProcessor::disconnect()
{
    Q_D(QnVMax480ConnectionProcessor);
    m_needStop = true;
    d->socket->close();
}
