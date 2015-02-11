#ifdef ENABLE_VMAX

#include "vmax480_tcp_server.h"

#include <QtCore/QElapsedTimer>
#include <utils/common/uuid.h>
#include <QtCore/QString>

#include "core/datapacket/audio_data_packet.h"
#include "core/datapacket/video_data_packet.h"
#include "vmax480_stream_fetcher.h"
#include "utils/network/tcp_connection_priv.h"
#include "../../../../vmaxproxy/src/vmax480_helper.h"

static const int UUID_LEN = 38;

// ---------------------------- QnVMax480ConnectionProcessor -----------------

QnVMax480Server::~QnVMax480Server()
{
    stop();
}


class QnVMax480ConnectionProcessorPrivate: public QnTCPConnectionProcessorPrivate
{
public:
    QString tcpID;
    QnMediaContextPtr context;
    VMaxStreamFetcher* streamFetcher;
    int openedChannels;
    static QnMutex connectMutex;
    QnTcpListener* owner;
};
QnMutex QnVMax480ConnectionProcessorPrivate::connectMutex;

QnVMax480ConnectionProcessor::QnVMax480ConnectionProcessor(QSharedPointer<AbstractStreamSocket> socket, QnTcpListener* _owner):
    QnTCPConnectionProcessor(new QnVMax480ConnectionProcessorPrivate, socket)
{
    Q_D(QnVMax480ConnectionProcessor);
    d->streamFetcher = 0;
    d->openedChannels = 0;
    d->owner = _owner;
}

QnVMax480ConnectionProcessor::~QnVMax480ConnectionProcessor()
{
    vMaxDisconnect();
    stop();
}

void QnVMax480ConnectionProcessor::vMaxConnect(const QString& url, int channel, const QAuthenticator& auth, bool isLive)
{
    Q_D(QnVMax480ConnectionProcessor);

    SCOPED_MUTEX_LOCK( lock, &d->connectMutex);

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

    SCOPED_MUTEX_LOCK( lock, &d->connectMutex);

    QElapsedTimer t;
    t.restart();
    if (d->socket->isConnected() && !d->tcpID.isEmpty()) 
    {
        QByteArray data = QnVMax480Helper::serializeCommand(Command_CloseConnect, 0, VMaxParamList());
        d->socket->send(data);
        quint8 dummy[1];
        d->socket->recv(dummy, sizeof(dummy));
    }
//    int waitTime = t.elapsed();
    d->socket->close();
}

void QnVMax480ConnectionProcessor::vMaxArchivePlay(qint64 timeUsec, quint8 sequence, int speed)
{
    Q_D(QnVMax480ConnectionProcessor);

    VMaxParamList params;
    params["pos"] = QString::number(timeUsec).toUtf8();
    params["speed"] = QString::number(speed).toUtf8();
    QByteArray data = QnVMax480Helper::serializeCommand(Command_ArchivePlay, sequence, params);
    qDebug () << "before send command vMaxArchivePlay" << "time=" << timeUsec << "sequence=" << sequence << "speed=" << speed;
    d->socket->send(data);
}

void QnVMax480ConnectionProcessor::vMaxAddChannel(int channel)
{
    Q_D(QnVMax480ConnectionProcessor);

    int newMask = d->openedChannels | channel;
    if (newMask == d->openedChannels)
        return;
    d->openedChannels = newMask;

    VMaxParamList params;
    params["channel"] = QString::number(channel).toUtf8();
    QByteArray data = QnVMax480Helper::serializeCommand(Command_AddChannel, 0, params);
    qDebug () << "before send command vMaxAddChannel";
    d->socket->send(data);
}

void QnVMax480ConnectionProcessor::vMaxRemoveChannel(int channel)
{
    Q_D(QnVMax480ConnectionProcessor);

    int newMask = d->openedChannels & ~channel;
    if (newMask == d->openedChannels)
        return;
    d->openedChannels = newMask;


    VMaxParamList params;
    params["channel"] = QString::number(channel).toUtf8();
    QByteArray data = QnVMax480Helper::serializeCommand(Command_RemoveChannel, 0, params);
    qDebug () << "before send command vMaxRemoveChannel";
    d->socket->send(data);
}

void QnVMax480ConnectionProcessor::vmaxPlayRange(const QList<qint64>& pointsUsec, quint8 sequence)
{
    Q_D(QnVMax480ConnectionProcessor);

    VMaxParamList params;
    QByteArray pointsData;

    for (int i = 0; i < pointsUsec.size(); ++i)
    {
        if (i > 0)
            pointsData.append(';');
        pointsData.append(QByteArray::number(quint32(pointsUsec[i]/1000000ll)));
    }

    params["points"] = pointsData;
    QByteArray data = QnVMax480Helper::serializeCommand(Command_PlayPoints, sequence, params);
    qDebug () << "before send command vmaxPlayRange" << "sequence=" << sequence;
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

void QnVMax480ConnectionProcessor::vMaxRequestRange()
{
    Q_D(QnVMax480ConnectionProcessor);

    VMaxParamList params;
    QByteArray data = QnVMax480Helper::serializeCommand(Command_GetRange, 0, params);

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
    //saveSysThreadID();

    char uuidBuffer[UUID_LEN+1];

    if (readBuffer((quint8*) uuidBuffer, UUID_LEN)) {
        uuidBuffer[UUID_LEN] = 0;
        d->tcpID = QString::fromUtf8(uuidBuffer);
        d->streamFetcher = static_cast<QnVMax480Server*>(d->owner)->bindConnection(d->tcpID, this);
    }

    if (!d->streamFetcher)
        return;

    while (!needToStop())
    {
        quint8 vMaxHeader[16];

        if (!readBuffer(vMaxHeader, sizeof(vMaxHeader)))
        {
            if (!d->socket->isConnected())
                break;    // connection closed
            else
                continue; // read timeout
        }

        quint8 sequence = vMaxHeader[0];
        VMaxDataType dataType = (VMaxDataType) vMaxHeader[1];

        quint8 internalCodecID = vMaxHeader[2];
        quint32 dataSize = *(quint32*)(vMaxHeader+4);
        quint64 timestamp = *(quint64*)(vMaxHeader+8);
        CodecID codecID = CODEC_ID_NONE;

        if (dataType == VMAXDT_GotArchiveRange)
        {
            quint32 startDateTime = *(quint32*)(vMaxHeader+8);
            quint32 endDateTime = *(quint32*)(vMaxHeader+12);
            d->streamFetcher->onGotArchiveRange(startDateTime, endDateTime);
            continue;
        }
        else if (dataType == VMAXDT_GotMonthInfo)
        {
            quint32 monthInfo = *(quint32*)(vMaxHeader+8);
            quint32 monthNum = *(quint32*)(vMaxHeader+12);
            QDate monthDate(monthNum/10000, (monthNum%10000)/100, 1);
            qDebug() << "month=" << monthDate.toString();
            d->streamFetcher->onGotMonthInfo(monthDate, monthInfo);
            continue;
        }
        else if (dataType == VMAXDT_GotDayInfo)
        {
            quint32 dayNum = *(quint32*)(vMaxHeader+12);
            QByteArray data;
            data.resize(VMAX_MAX_CH * (1440+60));
            if (!readBuffer((quint8*) data.data(), data.size()))
                break;

            d->streamFetcher->onGotDayInfo(dayNum, data);
            continue;
        }

        // media data

        QnAbstractMediaData* media = nullptr;
        QnByteArray* mediaBuffer = nullptr;
        if (dataType == VMAXDT_GotVideoPacket)
        {
            QnWritableCompressedVideoData* video = new QnWritableCompressedVideoData(CL_MEDIA_ALIGNMENT, dataSize);
            media = video;
            mediaBuffer = &video->m_data;
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
        else if (dataType == VMAXDT_GotAudioPacket)
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

            QnWritableCompressedAudioData* audio = new QnWritableCompressedAudioData(CL_MEDIA_ALIGNMENT, dataSize, d->context);
            media = audio;
            mediaBuffer = &audio->m_data;
        }

        if (!media)
            break;

        mediaBuffer->resize(mediaBuffer->capacity());
        if (!readBuffer((quint8*) mediaBuffer->data(), mediaBuffer->size()))
            break;

        if (vMaxHeader[3] & 1)
            media->flags = QnAbstractMediaData::MediaFlags_AVKey;
        media->channelNumber = vMaxHeader[3] >> 4;
        media->timestamp = timestamp;
        media->compressionType = codecID;
        media->opaque = sequence;

        d->streamFetcher->onGotData(QnAbstractMediaDataPtr(media));
    }

}

// ---------------------------- QnVMax480Server -------------------------

Q_GLOBAL_STATIC(QnVMax480Server, QnVMax480Server_instance)


QnVMax480Server* QnVMax480Server::instance()
{
    return QnVMax480Server_instance();
}

QnVMax480Server::QnVMax480Server(): QnTcpListener(QHostAddress(QLatin1String("127.0.0.1")), 0)
{
    start();
}

QString QnVMax480Server::registerProvider(VMaxStreamFetcher* provider)
{
    SCOPED_MUTEX_LOCK( lock, &m_mutexProvider);
    QString result = QnUuid::createUuid().toString();
    m_providers.insert(result, provider);
    return result;
}

void QnVMax480Server::unregisterProvider(VMaxStreamFetcher* provider)
{
    SCOPED_MUTEX_LOCK( lock, &m_mutexProvider);
    for (QMap<QString, VMaxStreamFetcher*>::iterator itr = m_providers.begin(); itr != m_providers.end(); ++itr)
    {
        if (itr.value() == provider) {
            m_providers.erase(itr);
            break;
        }
    }
}

VMaxStreamFetcher* QnVMax480Server::bindConnection(const QString& tcpID, QnVMax480ConnectionProcessor* connection)
{
    SCOPED_MUTEX_LOCK( lock, &m_mutexProvider);
    VMaxStreamFetcher* fetcher = m_providers.value(tcpID);
    if (fetcher) {
        removeOwnership(connection); // fetcher is going to take ownership
        fetcher->onConnectionEstablished(connection);
    }
    return fetcher;
}


QnTCPConnectionProcessor* QnVMax480Server::createRequestProcessor(QSharedPointer<AbstractStreamSocket> clientSocket, QnTcpListener* owner)
{
    return new QnVMax480ConnectionProcessor(clientSocket, owner);
}

#endif // #ifdef ENABLE_VMAX
