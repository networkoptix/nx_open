#include "vmax480_tcp_server.h"
#include "vmax480_stream_fetcher.h"
#include "utils/network/tcp_connection_priv.h"
#include "../../../../vmaxproxy/src/vmax480_helper.h"

static const int UUID_LEN = 38;

// ---------------------------- QnVMax480ConnectionProcessor -----------------

class QnVMax480ConnectionProcessorPrivate: public QnTCPConnectionProcessorPrivate
{
public:
    QString tcpID;
    QnMediaContextPtr context;
};

QnVMax480ConnectionProcessor::QnVMax480ConnectionProcessor(TCPSocket* socket, QnTcpListener* _owner):
    QnTCPConnectionProcessor(new QnVMax480ConnectionProcessorPrivate, socket, _owner)
{
    Q_D(QnVMax480ConnectionProcessor);
}

QnVMax480ConnectionProcessor::~QnVMax480ConnectionProcessor()
{
    stop();
}

void QnVMax480ConnectionProcessor::vMaxConnect(const QString& url, int channel, const QAuthenticator& auth, bool isLive)
{
    Q_D(QnVMax480ConnectionProcessor);

    VMaxParamList params;
    params["url"] = url.toUtf8();
    params["channel"] = QByteArray::number(channel);
    params["login"] = auth.user().toUtf8();
    params["password"] = auth.password().toUtf8();
    MServerCommand command = isLive ? Command_OpenLive : Command_OpenArchive;
    QByteArray data = QnVMax480Helper::serializeCommand(command, 0, params);

    d->socket->send(data);
}


void QnVMax480ConnectionProcessor::vMaxDisconnect()
{
    Q_D(QnVMax480ConnectionProcessor);
    d->socket->close();
}

void QnVMax480ConnectionProcessor::vMaxArchivePlay(qint64 timeUsec, quint8 sequence)
{
    Q_D(QnVMax480ConnectionProcessor);

    VMaxParamList params;
    params["pos"] = QString::number(timeUsec).toUtf8();
    QByteArray data = QnVMax480Helper::serializeCommand(Command_ArchivePlay, sequence, params);

    d->socket->send(data);
}

void QnVMax480ConnectionProcessor::vMaxRequestMonthInfo(const QDate& month)
{
    Q_D(QnVMax480ConnectionProcessor);

    int internalMonthFormat = month.year()*10000 + month.month()*100;
    VMaxParamList params;
    params["month"] = QString::number(internalMonthFormat).toUtf8();
    QByteArray data = QnVMax480Helper::serializeCommand(Command_RecordedMonth, 0, params);

    d->socket->send(data);
}

void QnVMax480ConnectionProcessor::vMaxRequestDayInfo(int dayNum)
{
    Q_D(QnVMax480ConnectionProcessor);

    VMaxParamList params;
    params["day"] = QString::number(dayNum).toUtf8();
    QByteArray data = QnVMax480Helper::serializeCommand(Command_RecordedTime, 0, params);

    d->socket->send(data);
}


bool QnVMax480ConnectionProcessor::readBuffer(quint8* buffer, int size)
{
    Q_D(QnVMax480ConnectionProcessor);

    int done = 0;
    while (!needToStop() && done < size) 
    {
        int readed = d->socket->recv(buffer + done, size - done);
        if (readed < 1)
            return false;
        done += readed;
    }
    return true;
}

void QnVMax480ConnectionProcessor::run()
{
    Q_D(QnVMax480ConnectionProcessor);

    char uuidBuffer[UUID_LEN+1];

    if (readBuffer((quint8*) uuidBuffer, UUID_LEN)) {
        uuidBuffer[UUID_LEN] = 0;
        d->tcpID = QString::fromUtf8(uuidBuffer);
        static_cast<QnVMax480Server*>(d->owner)->registerConnection(d->tcpID, this);
    }

    while (!needToStop())
    {
        quint8 vMaxHeader[16];

        if (!readBuffer(vMaxHeader, sizeof(vMaxHeader)))
            break;

        quint8 sequence = vMaxHeader[0];
        VMaxDataType dataType = (VMaxDataType) vMaxHeader[1];

        quint8 internalCodecID = vMaxHeader[2];
        quint32 dataSize = *(quint32*)(vMaxHeader+4);
        quint64 timestamp = *(quint64*)(vMaxHeader+8);
        CodecID codecID = CODEC_ID_NONE;

        QnAbstractMediaData* media = 0;

        if (dataType == VMAXDT_GotArchiveRange)
        {
            quint32 startDateTime = *(quint32*)(vMaxHeader+8);
            quint32 endDateTime = *(quint32*)(vMaxHeader+12);
            static_cast<QnVMax480Server*>(d->owner)->onGotArchiveRange(d->tcpID, startDateTime, endDateTime);
            continue;
        }
        else if (dataType == VMAXDT_GotMonthInfo)
        {
            quint32 monthInfo = *(quint32*)(vMaxHeader+8);
            quint32 monthNum = *(quint32*)(vMaxHeader+12);
            QDate monthDate(monthNum/10000, (monthNum%10000)/100, 1);
            qDebug() << "month=" << monthDate.toString();
            static_cast<QnVMax480Server*>(d->owner)->onGotMonthInfo(d->tcpID, monthDate, monthInfo);
            continue;
        }
        else if (dataType == VMAXDT_GotDayInfo)
        {
            quint32 dayNum = *(quint32*)(vMaxHeader+12);
            QByteArray data;
            data.resize(VMAX_MAX_CH * (1440+60));
            if (!readBuffer((quint8*) data.data(), data.size()))
                break;

            static_cast<QnVMax480Server*>(d->owner)->onGotDayInfo(d->tcpID, dayNum, data);
            continue;
        }

        // media data

        if (dataType == VMAXDT_GotVideoPacket)
        {
            QnCompressedVideoData* video = new QnCompressedVideoData(CL_MEDIA_ALIGNMENT, dataSize);
            media = video;
            switch (internalCodecID) {
                case CODEC_VSTREAM_H264:
                    codecID = CODEC_ID_H264;
                    break;
                case CODEC_VSTREAM_JPEG:
                    codecID = CODEC_ID_MJPEG;
                    break;
                case CODEC_VSTREAM_MPEG4:
                    codecID = CODEC_ID_MPEG4;
                    break;
            }
        }
        else if (dataSize == VMAXDT_GotAudioPacket)
        {
            quint8 vMaxAudioHeader[4];
            if (!readBuffer(vMaxAudioHeader, sizeof(vMaxAudioHeader)))
                break;

            AVSampleFormat sampleFormat = AV_SAMPLE_FMT_U8;

            switch (internalCodecID) {
                case CODEC_ASTREAM_MULAW:
                    codecID = CODEC_ID_PCM_MULAW;
                    break;
                case CODEC_ASTREAM_G723:
                    codecID = CODEC_ID_G723_1;
                    break;
                case CODEC_ASTREAM_IMAACPCM:
                    codecID = CODEC_ID_ADPCM_IMA_WAV;
                    break;
                case CODEC_ASTREAM_MSADPCM:
                    codecID = CODEC_ID_ADPCM_MS;
                    break;
                case CODEC_ASTREAM_PCM:
                    codecID = CODEC_ID_PCM_S8;
                    break;
                case CODEC_ASTREAM_CG726:
                    sampleFormat = AV_SAMPLE_FMT_S16;
                    codecID = CODEC_ID_ADPCM_G726;
                    break;
                case CODEC_ASTREAM_CG711A:
                    codecID = CODEC_ID_PCM_ALAW;
                    break;
                case CODEC_ASTREAM_CG711U:
                    codecID = CODEC_ID_PCM_MULAW;
                    break;
            }

            if (!d->context) {
                d->context = QnMediaContextPtr(new QnMediaContext(codecID));
                d->context->ctx()->channels = vMaxAudioHeader[0];
                d->context->ctx()->sample_rate = *(quint16*)(vMaxAudioHeader+2);
                d->context->ctx()->sample_fmt = sampleFormat;
                d->context->ctx()->time_base.num = 1;
                d->context->ctx()->bits_per_coded_sample = vMaxAudioHeader[1];
                d->context->ctx()->time_base.den = d->context->ctx()->sample_rate;
            }

            QnCompressedAudioData* audio = new QnCompressedAudioData(CL_MEDIA_ALIGNMENT, dataSize, d->context);
            media = audio;
        }

        if (!media)
            break;

        media->data.resize(media->data.capacity());
        if (!readBuffer((quint8*) media->data.data(), media->data.size()))
            break;

        if (vMaxHeader[3])
            media->flags = AV_PKT_FLAG_KEY;
        media->timestamp = timestamp;
        media->compressionType = codecID;
        media->opaque = sequence;

        static_cast<QnVMax480Server*>(d->owner)->onGotData(d->tcpID, QnAbstractMediaDataPtr(media));
    }

    if (!d->tcpID.isEmpty())
        static_cast<QnVMax480Server*>(d->owner)->unregisterConnection(d->tcpID);
}

// ---------------------------- QnVMax480Server -------------------------

Q_GLOBAL_STATIC(QnVMax480Server, inst)


QnVMax480Server* QnVMax480Server::instance()
{
    return inst();
}

QnVMax480Server::QnVMax480Server(): QnTcpListener(QHostAddress(QLatin1String("127.0.0.1")), 0)
{
    start();
}

QString QnVMax480Server::registerProvider(VMaxStreamFetcher* provider)
{
    QMutexLocker lock(&m_mutexProvider);
    QString result = QUuid::createUuid();
    m_providers.insert(result, provider);
    return result;
}

void QnVMax480Server::unregisterProvider(VMaxStreamFetcher* provider)
{
    QMutexLocker lock(&m_mutexProvider);
    for (QMap<QString, VMaxStreamFetcher*>::iterator itr = m_providers.begin(); itr != m_providers.end(); ++itr)
    {
        if (itr.value() == provider) {
            m_providers.erase(itr);
            break;
        }
    }
}

void QnVMax480Server::registerConnection(const QString& tcpID, QnVMax480ConnectionProcessor* connection)
{
    QMutexLocker lock(&m_mutexConnection);
    m_connections.insert(tcpID, connection);
}

void QnVMax480Server::unregisterConnection(const QString& tcpID)
{
    QMutexLocker lock(&m_mutexConnection);
    m_connections.remove(tcpID);
}

bool QnVMax480Server::waitForConnection(const QString& tcpID, int timeoutMs)
{
    QTime t;
    t.restart();
    do {
        {
            QMutexLocker lock(&m_mutexConnection);
            if (m_connections.contains(tcpID))
                return true;
        }
        msleep(5);
    } while (t.elapsed() < timeoutMs);
    
    return false;
}


QnTCPConnectionProcessor* QnVMax480Server::createRequestProcessor(TCPSocket* clientSocket, QnTcpListener* owner)
{
    return new QnVMax480ConnectionProcessor(clientSocket, owner);
}

void QnVMax480Server::vMaxConnect(const QString& tcpID, const QString& url, int channel, const QAuthenticator& auth, bool isLive)
{
    QMutexLocker lock(&m_mutexConnection);
    QnVMax480ConnectionProcessor* connection = m_connections.value(tcpID);
    if (connection)
        connection->vMaxConnect(url, channel, auth, isLive);
}

void QnVMax480Server::vMaxArchivePlay(const QString& tcpID, qint64 timeUsec, quint8 sequence)
{
    QMutexLocker lock(&m_mutexConnection);
    QnVMax480ConnectionProcessor* connection = m_connections.value(tcpID);
    if (connection)
        connection->vMaxArchivePlay(timeUsec, sequence);
}

void QnVMax480Server::vMaxRequestMonthInfo(const QString& tcpID, const QDate& month)
{
    QMutexLocker lock(&m_mutexConnection);
    QnVMax480ConnectionProcessor* connection = m_connections.value(tcpID);
    if (connection)
        connection->vMaxRequestMonthInfo(month);
}

void QnVMax480Server::vMaxRequestDayInfo(const QString& tcpID, int dayNum)
{
    QMutexLocker lock(&m_mutexConnection);
    QnVMax480ConnectionProcessor* connection = m_connections.value(tcpID);
    if (connection)
        connection->vMaxRequestDayInfo(dayNum);
}

void QnVMax480Server::vMaxDisconnect(const QString& tcpID)
{
    QMutexLocker lock(&m_mutexConnection);
    QnVMax480ConnectionProcessor* connection = m_connections.value(tcpID);
    if (connection)
        connection->vMaxDisconnect();
}

void QnVMax480Server::onGotData(const QString& tcpID, QnAbstractMediaDataPtr media)
{
    QMutexLocker lock(&m_mutexProvider);
    VMaxStreamFetcher* provider = m_providers.value(tcpID);
    if (provider)
        provider->onGotData(media);
}

void QnVMax480Server::onGotArchiveRange(const QString& tcpID, quint32 startDateTime, quint32 endDateTime)
{
    QMutexLocker lock(&m_mutexProvider);
    VMaxStreamFetcher* provider = m_providers.value(tcpID);
    if (provider)
        provider->onGotArchiveRange(startDateTime, endDateTime);
}

void QnVMax480Server::onGotMonthInfo(const QString& tcpID, QDate month, int monthInfo)
{
    QMutexLocker lock(&m_mutexProvider);
    VMaxStreamFetcher* provider = m_providers.value(tcpID);
    if (provider)
        provider->onGotMonthInfo(month, monthInfo);
}

void QnVMax480Server::onGotDayInfo(const QString& tcpID, int dayNum, const QByteArray& data)
{
    QMutexLocker lock(&m_mutexProvider);
    VMaxStreamFetcher* provider = m_providers.value(tcpID);
    if (provider)
        provider->onGotDayInfo(dayNum, data);
}
