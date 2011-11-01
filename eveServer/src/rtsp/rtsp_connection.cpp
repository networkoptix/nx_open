#include <QSet>
#include <QTextStream>
#include <QHttpRequestHeader>
#include <qDebug>

#include "libavutil/avutil.h"
#include "libavcodec/avcodec.h"

#include "rtsp_connection.h"
#include "utils/network/rtp_stream_parser.h"
#include "core//dataconsumer/dataconsumer.h"
#include "utils/media/ffmpeg_helper.h"
#include "core/dataprovider/media_streamdataprovider.h"
#include "core/resourcemanagment/resource_pool.h"
#include "core/resource/resource_media_layout.h"
#include "plugins/resources/archive/archive_stream_reader.h"

#include "utils/network/tcp_connection_priv.h"
#include "plugins/resources/archive/abstract_archive_delegate.h"
#include "camera/camera_pool.h"
#include "utils/network/rtpsession.h"


static const quint8 RTP_FFMPEG_GENERIC_CODE = 102;
static const QString RTP_FFMPEG_GENERIC_STR("FFMPEG");
//static const QString RTP_FFMPEG_GENERIC_STR("mpeg4-generic"); // this line for debugging purpose with VLC player
static const int MAX_QUEUE_SIZE = 15;
static const int MAX_RTSP_DATA_LEN = 65535 - 4 - RtpHeader::RTP_HEADER_SIZE;
static const int CLOCK_FREQUENCY = 1000000;
static const int RTSP_MIN_SEEK_INTERVAL = 1000 * 30; // 30 ms as min seek interval

static const int MAX_RTSP_WRITE_BUFFER = 1024*1024;

class QnTcpListener;

//#define DEBUG_RTSP

#ifdef DEBUG_RTSP
static void dumpRtspData(const char* data, int datasize)
{
    static QFile* binaryFile = 0;
    if (!binaryFile) {
        binaryFile = new QFile("c:/binary_server.rtsp");
        binaryFile->open(QFile::WriteOnly);
    }
    binaryFile->write(data, datasize);
    binaryFile->flush();
}
#endif

class QnRtspDataConsumer: public QnAbstractDataConsumer
{
public:
    QnRtspDataConsumer(QnRtspConnectionProcessor* owner):
      QnAbstractDataConsumer(MAX_QUEUE_SIZE),
      m_owner(owner),
      m_lastSendTime(0),
      m_waitBOF(false)
    {
        memset(m_sequence, 0, sizeof(m_sequence));
        m_timer.start();
    }
    //qint64 lastSendTime() const { return m_lastSendTime; }
    void setLastSendTime(qint64 time) { m_lastSendTime = time; }
    void setWaitBOF(qint64 newTime, bool value) 
    { 
        QMutexLocker lock(&m_mutex);
        m_waitBOF = value; 
        m_lastSendTime = newTime;
        m_ctxSended.clear();
    }

    virtual qint64 currentTime() const { 
        return m_lastSendTime; 
    }

protected:


    void buildRtspTcpHeader(quint8 channelNum, quint32 ssrc, quint16 len, int markerBit, quint32 timestamp)
    {
        m_rtspTcpHeader[0] = '$';
        m_rtspTcpHeader[1] = channelNum;
        quint16* lenPtr = (quint16*) &m_rtspTcpHeader[2];
        *lenPtr = htons(len+sizeof(RtpHeader));
        RtpHeader* rtp = (RtpHeader*) &m_rtspTcpHeader[4];
        rtp->version = RtpHeader::RTP_VERSION;
        rtp->padding = 0;
        rtp->extension = 0;
        rtp->CSRCCount = 0;
        rtp->marker  =  markerBit;
        rtp->payloadType = RTP_FFMPEG_GENERIC_CODE;
        rtp->sequence = htons(m_sequence[channelNum]++);
        //rtp->timestamp = htonl(m_timer.elapsed());
        rtp->timestamp = htonl(timestamp);
        rtp->ssrc = htonl(ssrc); // source ID
    }

    virtual bool processData(QnAbstractDataPacketPtr data)
    {
        QnAbstractMediaDataPtr media = qSharedPointerDynamicCast<QnAbstractMediaData>(data);
        int rtspChannelNum = media->channelNumber;
        if (media->dataType == QnAbstractMediaData::AUDIO)
            rtspChannelNum += m_owner->numOfVideoChannels();

        QMutexLocker lock(&m_mutex);
        if (media->flags & QnAbstractMediaData::MediaFlags_AfterEOF)
            m_ctxSended.clear();

        if (m_waitBOF && !(media->flags & QnAbstractMediaData::MediaFlags_BOF))
        {
            return true; // ignore data
        }
        m_waitBOF = false;


        AVCodecContext* ctx = (AVCodecContext*) media->context;
        //if (!ctx)
        //    return true;

        // one video channel may has several subchannels (video combined with frames from difference codecContext)
        // max amount of subchannels is MAX_CONTEXTS_AT_VIDEO. Each channel used 2 ssrc: for data and for CodecContext
        
        quint32 ssrc = BASIC_FFMPEG_SSRC + rtspChannelNum * MAX_CONTEXTS_AT_VIDEO*2;

        ssrc += media->subChannelNumber*2;
        int subChannelNumber = media->subChannelNumber;
        if (ctx && !m_ctxSended[rtspChannelNum].contains(subChannelNumber))
        {
            QByteArray codecCtxData;
            QnFfmpegHelper::serializeCodecContext(ctx, &codecCtxData);
            buildRtspTcpHeader(rtspChannelNum, ssrc + 1, codecCtxData.size(), true, 0); // ssrc+1 - switch data subchannel to context subchannel
            QMutexLocker lock(&m_owner->getSockMutex());
            m_owner->sendData(m_rtspTcpHeader, sizeof(m_rtspTcpHeader));
            m_owner->sendData(codecCtxData);
            m_ctxSended[rtspChannelNum] << media->subChannelNumber;
        }
        /*
        int subChannelNumber = ctx ? m_ctxSended[rtspChannelNum].indexOf(ctx) : 0;
        // serialize and send FFMPEG context to stream
        if (subChannelNumber == -1)
        {
            subChannelNumber = m_ctxSended[rtspChannelNum].size();
            ssrc += subChannelNumber*2;
            QByteArray codecCtxData;
            QnFfmpegHelper::serializeCodecContext(ctx, &codecCtxData);
            buildRtspTcpHeader(rtspChannelNum, ssrc + 1, codecCtxData.size(), true, 0); // ssrc+1 - switch data subchannel to context subchannel
            QMutexLocker lock(&m_owner->getSockMutex());
            m_owner->sendData(m_rtspTcpHeader, sizeof(m_rtspTcpHeader));
            m_owner->sendData(codecCtxData);
            m_ctxSended[rtspChannelNum] << ctx;
        }
        else
        {
            ssrc += subChannelNumber*2;
        }
        */

        // send data with RTP headers
        QnCompressedVideoData *video = media.dynamicCast<QnCompressedVideoData>().data();
        const char* curData = media->data.data();
        int sendLen = 0;
        int headerSize = 4 + (video ? 4 : 0);
        for (int dataRest = media->data.size(); dataRest > 0; dataRest -= sendLen)
        {
            sendLen = qMin(MAX_RTSP_DATA_LEN-headerSize, dataRest);
            buildRtspTcpHeader(rtspChannelNum, ssrc, sendLen + headerSize, sendLen >= dataRest ? 1 : 0, media->timestamp);
            QMutexLocker lock(&m_owner->getSockMutex());
            m_owner->sendData(m_rtspTcpHeader, sizeof(m_rtspTcpHeader));
            if (headerSize) 
            {
                quint32 timestampHigh = htonl(media->timestamp >> 32);
                m_owner->sendData((const char*) &timestampHigh, 4);
                if (video) 
                {
                    quint32 videoHeader = htonl((video->flags << 24) + (video->data.size() & 0x00ffffff));
                    m_owner->sendData((const char*) &videoHeader, 4);
                }
                headerSize = 0;
            }
            m_owner->sendData(curData, sendLen);
            //m_owner->flush();
            curData += sendLen;
            m_lastSendTime = media->timestamp;
        }
        return true;
    }

private:
    QByteArray m_codecCtxData;
    //QMap<int, QList<AVCodecContext*> > m_ctxSended;
    QMap<int, QList<int> > m_ctxSended;
    QTime m_timer;
    quint16 m_sequence[256];
    QnRtspConnectionProcessor* m_owner;
    qint64 m_lastSendTime;
    char m_rtspTcpHeader[4 + RtpHeader::RTP_HEADER_SIZE];
    quint8* tcpReadBuffer;
    QMutex m_mutex;
    bool m_waitBOF;
};

// ----------------------------- QnRtspConnectionProcessorPrivate ----------------------------

class QnRtspConnectionProcessor::QnRtspConnectionProcessorPrivate: public QnTCPConnectionProcessor::QnTCPConnectionProcessorPrivate
{
public:
    //enum State {State_Stopped, State_Paused, State_Playing, State_Rewind};

    QnRtspConnectionProcessorPrivate():
        QnTCPConnectionProcessor::QnTCPConnectionProcessorPrivate(),
        liveDP(0),
        archiveDP(0),
        dataProcessor(0),
        startTime(0),
        endTime(0),
        rtspScale(1.0),
        liveMode(false)
    {
    }
    void deleteDP()
    {
        if (archiveDP)
            archiveDP->stop();
        if (dataProcessor)
            dataProcessor->stop();

        if (liveDP)
            liveDP->removeDataProcessor(dataProcessor);
        if (archiveDP)
            archiveDP->removeDataProcessor(dataProcessor);
        delete archiveDP;
        delete dataProcessor;
        archiveDP = 0;
        dataProcessor = 0;
    }


    ~QnRtspConnectionProcessorPrivate()
    {
        deleteDP();
    }

    QnAbstractMediaStreamDataProvider* liveDP;
    QnAbstractArchiveReader* archiveDP;
    bool liveMode;

    QnRtspDataConsumer* dataProcessor;

    QString sessionId;
    QnMediaResourcePtr mediaRes;
    // associate trackID with RTP/RTCP ports (for TCP mode ports used as logical channel numbers, see RFC 2326)
    QMap<int, QPair<int,int> > trackPorts;
    qint64 startTime; // time from last range header
    qint64 endTime;   // time from last range header
    double rtspScale; // RTSP playing speed (1 - normal speed, 0 - pause, >1 fast forward, <-1 fast back e. t.c.)
};

// ----------------------------- QnRtspConnectionProcessor ----------------------------

QnRtspConnectionProcessor::QnRtspConnectionProcessor(TCPSocket* socket, QnTcpListener* _owner):
    QnTCPConnectionProcessor(new QnRtspConnectionProcessorPrivate, socket, _owner)
{
}

QnRtspConnectionProcessor::~QnRtspConnectionProcessor()
{
}

void QnRtspConnectionProcessor::parseRequest()
{
    Q_D(QnRtspConnectionProcessor);
    QnTCPConnectionProcessor::parseRequest();

    processRangeHeader();

    if (d->mediaRes == 0)
    {
        QString resId = extractPath();
        if (resId.startsWith('/'))
            resId = resId.mid(1);
        QnResourcePtr resource = qnResPool->getResourceById(resId);
        if (resource == 0)
            resource = qnResPool->getResourceByUrl(resId);
        d->mediaRes = qSharedPointerDynamicCast<QnMediaResource>(resource);
    }
    d->clientRequest.clear();
}

void QnRtspConnectionProcessor::initResponse(int code, const QString& message)
{
    Q_D(QnRtspConnectionProcessor);
    d->responseBody.clear();
    d->responseHeaders = QHttpResponseHeader(code, message, d->requestHeaders.majorVersion(), d->requestHeaders.minorVersion());
    d->responseHeaders.addValue("CSeq", d->requestHeaders.value("CSeq"));
    QString transport = d->requestHeaders.value("Transport");
    if (!transport.isEmpty())
        d->responseHeaders.addValue("Transport", transport);

    if (!d->sessionId.isEmpty())
        d->responseHeaders.addValue("Session", d->sessionId);
}

void QnRtspConnectionProcessor::generateSessionId()
{
    Q_D(QnRtspConnectionProcessor);
    d->sessionId = QString::number((long) d->socket);
    d->sessionId += QString::number(rand());
}

void QnRtspConnectionProcessor::sendResponse(int code)
{
    QnTCPConnectionProcessor::sendResponse("RTSP", code, "application/sdp");
}

int QnRtspConnectionProcessor::numOfVideoChannels()
{
    Q_D(QnRtspConnectionProcessor);
    if (!d->mediaRes)
        return -1;
    QnAbstractMediaStreamDataProvider* currentDP = d->liveMode ? d->liveDP : d->archiveDP;
    QnVideoResourceLayout* layout = currentDP->getVideoLayout();
    return layout ? layout->numberOfChannels() : -1;
}

int QnRtspConnectionProcessor::composeDescribe()
{
    Q_D(QnRtspConnectionProcessor);
    if (!d->mediaRes)
        return CODE_NOT_FOUND;

    createDataProvider();

    QString acceptMethods = d->requestHeaders.value("Accept");
    if (acceptMethods.indexOf("sdp") == -1)
        return CODE_NOT_IMPLEMETED;

    QTextStream sdp(&d->responseBody);

    QnVideoResourceLayout* videoLayout = d->liveDP->getVideoLayout();
    QnResourceAudioLayout* audioLayout = d->liveDP->getAudioLayout();
    int numVideo = videoLayout ? videoLayout->numberOfChannels() : 1;
    int numAudio = audioLayout ? audioLayout->numberOfChannels() : 0;

    if (d->archiveDP) {
        QString range = "npt=";
        range += QString::number(d->archiveDP->startTime());
        range += "-";
        range += QString::number(d->archiveDP->endTime());
        d->responseHeaders.addValue("Range", range);
    }

    for (int i = 0; i < numVideo + numAudio; ++i)
    {
        sdp << "m=" << (i < numVideo ? "video " : "audio ") << i << " RTP/AVP " << RTP_FFMPEG_GENERIC_CODE << ENDL;
        sdp << "a=control:trackID=" << i << ENDL;
        sdp << "a=rtpmap:" << RTP_FFMPEG_GENERIC_CODE << ' ' << RTP_FFMPEG_GENERIC_STR << "/" << CLOCK_FREQUENCY << ENDL;
    }
    return CODE_OK;
}

int QnRtspConnectionProcessor::extractTrackId(const QString& path)
{
    int pos = path.lastIndexOf("/");
    QString trackStr = path.mid(pos+1);
    if (trackStr.toLower().startsWith("trackid"))
    {
        QStringList data = trackStr.split("=");
        if (data.size() > 1)
            return data[1].toInt();
    }
    return -1;
}

int QnRtspConnectionProcessor::composeSetup()
{
    Q_D(QnRtspConnectionProcessor);
    if (!d->mediaRes)
        return CODE_NOT_FOUND;

    QString transport = d->requestHeaders.value("Transport");
    if (transport.indexOf("TCP") == -1)
        return CODE_NOT_IMPLEMETED;
    int trackId = extractTrackId(d->requestHeaders.path());

    QnAbstractMediaStreamDataProvider* currentDP = d->liveMode ? d->liveDP : d->archiveDP;
    QnVideoResourceLayout* videoLayout = currentDP->getVideoLayout();
    if (trackId >= videoLayout->numberOfChannels()) {
        //QnAbstractMediaStreamDataProvider* dataProvider;
        if (d->archiveDP)
            d->archiveDP->setAudioChannel(trackId - videoLayout->numberOfChannels());
    }

    if (trackId >= 0)
    {
        QStringList transportInfo = transport.split(';');
        foreach(const QString& data, transportInfo)
        {
            if (data.startsWith("interleaved="))
            {
                QStringList ports = data.mid(QString("interleaved=").length()).split("-");
                d->trackPorts.insert(trackId, QPair<int,int>(ports[0].toInt(), ports.size() > 1 ? ports[1].toInt() : 0));
            }
        }
    }
    return CODE_OK;
}

int QnRtspConnectionProcessor::composePause()
{
    Q_D(QnRtspConnectionProcessor);
    //if (!d->dataProvider)
    //    return CODE_NOT_FOUND;
    if (d->archiveDP)
        d->archiveDP->pause();

    //d->playTime += d->rtspScale * d->playTimer.elapsed()*1000;
    d->rtspScale = 0;

    //d->state = QnRtspConnectionProcessorPrivate::State_Paused;
    return CODE_OK;
}

void QnRtspConnectionProcessor::setRtspTime(qint64 time)
{
    Q_D(QnRtspConnectionProcessor);
    return d->dataProcessor->setLastSendTime(time);
}

qint64 QnRtspConnectionProcessor::getRtspTime()
{
    Q_D(QnRtspConnectionProcessor);
    //return d->playTime + d->playTimer.elapsed()*1000 * d->rtspScale;
    return d->dataProcessor->currentTime();
}

void QnRtspConnectionProcessor::extractNptTime(const QString& strValue, qint64* dst)
{
    Q_D(QnRtspConnectionProcessor);
    d->liveMode = strValue == "now";
    if (d->liveMode)
    {
        //*dst = getRtspTime();
        *dst = RTPSession::RTSP_NOW;
    }
    else {
        double val = strValue.toDouble();
        // some client got time in seconds, some in microseconds, convert all to microseconds
        *dst = val < 1000000 ? val * 1000000.0 : val;
    }
}

void QnRtspConnectionProcessor::processRangeHeader()
{
    Q_D(QnRtspConnectionProcessor);
    QString rangeStr = d->requestHeaders.value("Range");
    if (rangeStr.isNull())
        return;
    parseRangeHeader(rangeStr, &d->startTime, &d->endTime);
}

void QnRtspConnectionProcessor::parseRangeHeader(const QString& rangeStr, qint64* startTime, qint64* endTime)
{
    QStringList rangeType = rangeStr.trimmed().split("=");
    if (rangeType.size() < 2)
        return;
    if (rangeType[0] == "npt")
    {
        QStringList values = rangeType[1].split("-");

        extractNptTime(values[0], startTime);
        if (values.size() > 1 && !values[1].isEmpty())
            extractNptTime(values[1], endTime);
        else
            *endTime = RTPSession::RTSP_NOW;
    }
}

void QnRtspConnectionProcessor::createDataProvider()
{
    Q_D(QnRtspConnectionProcessor);
    if (!d->liveDP) {
        QnVideoCamera* camera = qnCameraPool->getVideoCamera(d->mediaRes);
        if (camera)
            d->liveDP = camera->getLiveReader();
    }
    if (!d->archiveDP) {
        d->archiveDP = dynamic_cast<QnAbstractArchiveReader*> (d->mediaRes->createDataProvider(QnResource::Role_Archive));
    }
}

int QnRtspConnectionProcessor::composePlay()
{
    Q_D(QnRtspConnectionProcessor);
    if (d->mediaRes == 0)
        return CODE_NOT_FOUND;
    createDataProvider();

    if (d->archiveDP)
        d->archiveDP->pause();

    QnAbstractMediaStreamDataProvider* currentDP = d->liveMode ? d->liveDP : d->archiveDP;
    if (!currentDP)
        return CODE_NOT_FOUND;

    if (!d->requestHeaders.value("Scale").isNull())
        d->rtspScale = d->requestHeaders.value("Scale").toDouble();

    if (d->liveDP && !d->liveMode)
        d->liveDP->removeDataProcessor(d->dataProcessor);

    if (!d->dataProcessor) 
        d->dataProcessor = new QnRtspDataConsumer(this);
    
    currentDP->addDataProcessor(d->dataProcessor);
    

    //QnArchiveStreamReader* archiveProvider = dynamic_cast<QnArchiveStreamReader*> (d->dataProvider);
    if (qAbs(d->startTime - getRtspTime()) >= RTSP_MIN_SEEK_INTERVAL)
    {
        if (d->archiveDP)
        {
            d->dataProcessor->clearUnprocessedData();
            d->dataProcessor->setWaitBOF(d->startTime, true); // ignore rest packets before new position
            d->archiveDP->jumpTo(d->startTime, true);
        }
        else {
            qWarning() << "Seek operation not supported for dataProvider" << d->mediaRes->getId();
        }
    }

    if (!d->liveMode) {
        d->archiveDP->setReverseMode(d->rtspScale < 0);
        d->archiveDP->resume();
    }
    return CODE_OK;
}

int QnRtspConnectionProcessor::composeTeardown()
{
    Q_D(QnRtspConnectionProcessor);
    d->mediaRes = QnMediaResourcePtr(0);

    d->deleteDP();

    d->rtspScale = 1.0;
    d->startTime = d->endTime = 0;

    d->trackPorts.clear();

    return CODE_OK;
}

int QnRtspConnectionProcessor::composeSetParameter()
{
    return CODE_INVALID_PARAMETER;
}

int QnRtspConnectionProcessor::composeGetParameter()
{
    Q_D(QnRtspConnectionProcessor);
    QList<QByteArray> parameters = d->requestBody.split('\n');
    foreach(const QByteArray& parameter, parameters)
    {
        if (parameter.trimmed().toLower() == "position")
        {
            d->responseBody.append("position: ");
            d->responseBody.append(QString::number(getRtspTime()));
            d->responseBody.append(ENDL);
        }
        else {
            qWarning() << Q_FUNC_INFO << __LINE__ << "Unsupported RTSP parameter " << parameter.trimmed();
            return CODE_INVALID_PARAMETER;
        }
    }

    return CODE_OK;
}

void QnRtspConnectionProcessor::processRequest()
{
    Q_D(QnRtspConnectionProcessor);
    QString method = d->requestHeaders.method();
    if (method != "OPTIONS" && d->sessionId.isEmpty())
        generateSessionId();
    int code = CODE_OK;
    initResponse();
    if (method == "OPTIONS")
    {
        d->responseHeaders.addValue("Public", "DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE, GET_PARAMETER, SET_PARAMETER");
    }
    else if (method == "DESCRIBE")
    {
        code = composeDescribe();
    }
    else if (method == "SETUP")
    {
        code = composeSetup();
    }
    else if (method == "PLAY")
    {
        code = composePlay();
    }
    else if (method == "PAUSE")
    {
        code = composePause();
    }
    else if (method == "TEARDOWN")
    {
        code = composeTeardown();
    }
    else if (method == "GET_PARAMETER")
    {
        code = composeGetParameter();
    }
    else if (method == "SET_PARAMETER")
    {
        code = composeSetParameter();
    }
    sendResponse(code);
    QnAbstractStreamDataProvider* currentDP = d->liveMode ? d->liveDP : d->archiveDP;
    if (currentDP && !currentDP->isRunning())
        currentDP->start();
    if (d->dataProcessor && !d->dataProcessor->isRunning())
        d->dataProcessor->start();
}

void QnRtspConnectionProcessor::run()
{
    Q_D(QnRtspConnectionProcessor);
    while (!m_needStop && d->socket->isConnected())
    {
        int readed = d->socket->recv(d->tcpReadBuffer, TCP_READ_BUFFER_SIZE);
        if (readed > 0) {
            d->clientRequest.append((const char*) d->tcpReadBuffer, readed);
            if (isFullMessage())
            {
                parseRequest();
                processRequest();
            }
        }
    }
    if (d->dataProcessor)
        d->dataProcessor->stop();
    if (d->archiveDP)
        d->archiveDP->stop();
    delete d->archiveDP;
    delete d->dataProcessor;
    d->archiveDP = 0;
    d->liveDP = 0;
    d->dataProcessor = 0;
    m_runing = false;
    //deleteLater(); // does not works for this thread
}
