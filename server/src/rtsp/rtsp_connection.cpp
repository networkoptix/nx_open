#include <QSet>
#include <QTextStream>
#include <QHttpRequestHeader>
#include <qDebug>

#include "libavutil/avutil.h"
#include "libavcodec/avcodec.h"

#include "rtsp_connection.h"
#include "utils/network/rtp_stream_parser.h"
#include "core/dataconsumer/dataconsumer.h"
#include "utils/media/ffmpeg_helper.h"
#include "core/dataprovider/media_streamdataprovider.h"
#include "core/resourcemanagment/resource_pool.h"
#include "core/resource/resource_media_layout.h"
#include "plugins/resources/archive/archive_stream_reader.h"

#include "utils/network/tcp_connection_priv.h"
#include "plugins/resources/archive/abstract_archive_delegate.h"
#include "camera/camera_pool.h"
#include "utils/network/rtpsession.h"
#include "recorder/recording_manager.h"
#include "utils/common/util.h"
#include "rtsp_data_consumer.h"

class QnTcpListener;

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
        liveMode(false),
        gotLivePacket(false)
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
    QMutex mutex;
    bool gotLivePacket;
};

// ----------------------------- QnRtspConnectionProcessor ----------------------------

QnRtspConnectionProcessor::QnRtspConnectionProcessor(TCPSocket* socket, QnTcpListener* _owner):
    QnTCPConnectionProcessor(new QnRtspConnectionProcessorPrivate, socket, _owner)
{
}

QnRtspConnectionProcessor::~QnRtspConnectionProcessor()
{
}

bool QnRtspConnectionProcessor::isLiveDP(QnAbstractStreamDataProvider* dp)
{
    Q_D(QnRtspConnectionProcessor);
    return dp == d->liveDP;
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
        QnResourcePtr resource = qnResPool->getResourceByUrl(resId);
        if (resource == 0) {
            resource = qnResPool->getNetResourceByMac(resId);
        }
        d->mediaRes = qSharedPointerDynamicCast<QnMediaResource>(resource);
    }
    d->clientRequest.clear();
}

QnMediaResourcePtr QnRtspConnectionProcessor::getResource() const
{
    Q_D(const QnRtspConnectionProcessor);
    return d->mediaRes;

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
    const QnVideoResourceLayout* layout = d->mediaRes->getVideoLayout(currentDP);
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

    
    const QnVideoResourceLayout* videoLayout = d->mediaRes->getVideoLayout(d->liveDP);
    const QnResourceAudioLayout* audioLayout = d->mediaRes->getAudioLayout(d->liveDP);
    int numVideo = videoLayout ? videoLayout->numberOfChannels() : 1;
    int numAudio = audioLayout ? audioLayout->numberOfChannels() : 0;

    if (d->archiveDP) {
        QString range = "npt=";
        d->archiveDP->open();
        range += QString::number(d->archiveDP->startTime());
        range += "-";
        if (QnRecordingManager::instance()->isCameraRecoring(d->mediaRes))
            range += "now";
        else
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
    const QnVideoResourceLayout* videoLayout = d->mediaRes->getVideoLayout(currentDP);
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
        *dst = DATETIME_NOW;
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
            *endTime = DATETIME_NOW;
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

    if (!d->dataProcessor) 
        d->dataProcessor = new QnRtspDataConsumer(this);
    else 
        d->dataProcessor->clearUnprocessedData();
    //d->dataProcessor->pause();

    if (d->liveMode && d->archiveDP)
        d->archiveDP->stop();
    else if (d->liveDP && !d->liveMode)
        d->liveDP->removeDataProcessor(d->dataProcessor);


    QnAbstractMediaStreamDataProvider* currentDP = d->liveMode ? d->liveDP : d->archiveDP;
    if (!currentDP)
        return CODE_NOT_FOUND;


    if (!d->requestHeaders.value("Scale").isNull())
        d->rtspScale = d->requestHeaders.value("Scale").toDouble();


    d->dataProcessor->setLiveMode(d->liveMode);
    currentDP->addDataProcessor(d->dataProcessor);
    
    //QnArchiveStreamReader* archiveProvider = dynamic_cast<QnArchiveStreamReader*> (d->dataProvider);
    if (d->liveMode) {
        d->dataProcessor->setWaitBOF(d->startTime, false); // ignore rest packets before new position
        d->dataProcessor->copyLastGopFromCamera();
    }
    else if (qAbs(d->startTime - getRtspTime()) >= RTSP_MIN_SEEK_INTERVAL)
    {
        if (d->archiveDP)
        {
            //d->dataProcessor->clearUnprocessedData();
            d->dataProcessor->setWaitBOF(d->startTime, true); // ignore rest packets before new position
            d->archiveDP->jumpTo(d->startTime, true);
        }
        else {
            qWarning() << "Seek operation not supported for dataProvider" << d->mediaRes->getId();
        }
    }
    currentDP->start();
    d->dataProcessor->start();
    //d->dataProcessor->resume();

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
    QMutexLocker lock(&d->mutex);

    if (d->dataProcessor)
        d->dataProcessor->pauseNetwork();


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
    if (d->dataProcessor)
        d->dataProcessor->resumeNetwork();
}

void QnRtspConnectionProcessor::run()
{
    Q_D(QnRtspConnectionProcessor);
    QTime t;
    while (!m_needStop && d->socket->isConnected())
    {
        t.restart();
        int readed = d->socket->recv(d->tcpReadBuffer, TCP_READ_BUFFER_SIZE);
        if (readed > 0) {
            d->clientRequest.append((const char*) d->tcpReadBuffer, readed);
            if (isFullMessage())
            {
                parseRequest();
                processRequest();
            }
        }
        else if (t.elapsed() < 2)
        {
            // recv return 0 bytes imediatly, so client has closed socket
            break;
        }
    }

    d->deleteDP();
    d->socket->close();
    m_runing = false;
    //deleteLater(); // does not works for this thread
}

void QnRtspConnectionProcessor::switchToLive()
{
    Q_D(QnRtspConnectionProcessor);
    QMutexLocker lock(&d->mutex);
    d->liveMode = true;
    composePlay();
}
