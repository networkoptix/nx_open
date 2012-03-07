#include <QSet>
#include <QTextStream>
#include <QHttpRequestHeader>
#include <QDebug>
#include <QRegion>
#include <QBuffer>

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
#include "device_plugins/server_archive/server_archive_delegate.h"

class QnTcpListener;

// ----------------------------- QnRtspConnectionProcessorPrivate ----------------------------

class QnRtspConnectionProcessor::QnRtspConnectionProcessorPrivate: public QnTCPConnectionProcessor::QnTCPConnectionProcessorPrivate
{
public:
    //enum State {State_Stopped, State_Paused, State_Playing, State_Rewind};

    QnRtspConnectionProcessorPrivate():
        QnTCPConnectionProcessor::QnTCPConnectionProcessorPrivate(),
        liveDpHi(0),
        liveDpLow(0),
        archiveDP(0),
        dataProcessor(0),
        startTime(0),
        endTime(0),
        rtspScale(1.0),
        liveMode(false),
        gotLivePacket(false),
        lastPlayCSeq(0),
        quality(MEDIA_Quality_High),
        qualityFastSwitch(true),
        prevStartTime(AV_NOPTS_VALUE),
        prevEndTime(AV_NOPTS_VALUE)
    {
    }
    void deleteDP()
    {
        if (archiveDP)
            archiveDP->stop();
        if (dataProcessor)
            dataProcessor->stop();

        if (liveDpHi)
            liveDpHi->removeDataProcessor(dataProcessor);
        if (liveDpLow)
            liveDpLow->removeDataProcessor(dataProcessor);
        if (archiveDP)
            archiveDP->removeDataProcessor(dataProcessor);
        delete archiveDP;
        delete dataProcessor;
        archiveDP = 0;
        dataProcessor = 0;

        QnVideoCamera* camera = qnCameraPool->getVideoCamera(mediaRes);
		if (camera)
			camera->notInUse(this);
    }

    ~QnRtspConnectionProcessorPrivate()
    {
        deleteDP();
    }

    QnAbstractMediaStreamDataProvider* liveDpHi;
    QnAbstractMediaStreamDataProvider* liveDpLow;
    QnArchiveStreamReader* archiveDP;
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
    int lastPlayCSeq;
    MediaQuality quality;
    bool qualityFastSwitch;
    qint64 prevStartTime;
    qint64 prevEndTime;
};

// ----------------------------- QnRtspConnectionProcessor ----------------------------

QnRtspConnectionProcessor::QnRtspConnectionProcessor(TCPSocket* socket, QnTcpListener* _owner):
    QnTCPConnectionProcessor(new QnRtspConnectionProcessorPrivate, socket, _owner)
{
}

QnRtspConnectionProcessor::~QnRtspConnectionProcessor()
{
    stop();
}

void QnRtspConnectionProcessor::parseRequest()
{
    Q_D(QnRtspConnectionProcessor);
    QnTCPConnectionProcessor::parseRequest();

    if (!d->requestHeaders.value("Scale").isNull())
        d->rtspScale = d->requestHeaders.value("Scale").toDouble();
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
    if (d->requestHeaders.value("x-media-quality") == QString("low"))
        d->quality = MEDIA_Quality_Low;
    else
        d->quality = MEDIA_Quality_High;
    d->qualityFastSwitch = true;
    d->clientRequest.clear();
}

QnMediaResourcePtr QnRtspConnectionProcessor::getResource() const
{
    Q_D(const QnRtspConnectionProcessor);
    return d->mediaRes;

}

bool QnRtspConnectionProcessor::isLiveDP(QnAbstractStreamDataProvider* dp)
{
    Q_D(QnRtspConnectionProcessor);
    return dp == d->liveDpHi || dp == d->liveDpLow;
}

bool QnRtspConnectionProcessor::isSecondaryLiveDP(QnAbstractStreamDataProvider* dp) const
{
    Q_D(const QnRtspConnectionProcessor);
    return dp == d->liveDpLow;
}

QHostAddress QnRtspConnectionProcessor::getPeerAddress() const
{
    Q_D(const QnRtspConnectionProcessor);
    return QHostAddress(d->socket->getPeerAddressUint());
}

bool QnRtspConnectionProcessor::isPrimaryLiveDP(QnAbstractStreamDataProvider* dp) const
{
    Q_D(const QnRtspConnectionProcessor);
    return dp == d->liveDpHi;
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

void QnRtspConnectionProcessor::sendCurrentRangeIfUpdated()
{
    Q_D(QnRtspConnectionProcessor);
    QMutexLocker lock(&d->mutex);

    if (!d->archiveDP)
        return;

    qint64 endTime = d->archiveDP->endTime();
    if (QnRecordingManager::instance()->isCameraRecoring(d->mediaRes)) 
        endTime = DATETIME_NOW;

    if (d->archiveDP->startTime() != d->prevStartTime || endTime != d->prevEndTime)
    {
        addResponseRangeHeader();
        sendResponse(CODE_OK);
    }
};

void QnRtspConnectionProcessor::sendResponse(int code)
{
    QnTCPConnectionProcessor::sendResponse("RTSP", code, "application/sdp");
}

int QnRtspConnectionProcessor::numOfVideoChannels()
{
    Q_D(QnRtspConnectionProcessor);
    if (!d->mediaRes)
        return -1;
    QnAbstractMediaStreamDataProvider* currentDP = d->liveMode ? d->liveDpHi : d->archiveDP;
    const QnVideoResourceLayout* layout = d->mediaRes->getVideoLayout(currentDP);
    return layout ? layout->numberOfChannels() : -1;
}

void QnRtspConnectionProcessor::addResponseRangeHeader()
{
    Q_D(QnRtspConnectionProcessor);
    if (d->archiveDP) 
    {
        QString range = "npt=";
        d->archiveDP->open();
        if (d->archiveDP->startTime() == AV_NOPTS_VALUE)
            range += "now";
        else
            range += QString::number(d->archiveDP->startTime());
        d->prevStartTime = d->archiveDP->startTime();

        range += "-";
        if (QnRecordingManager::instance()->isCameraRecoring(d->mediaRes)) {
            range += "now";
            d->prevEndTime = DATETIME_NOW;
        }
        else {
            range += QString::number(d->archiveDP->endTime());
            d->prevEndTime = d->archiveDP->endTime();
        }
        d->responseHeaders.removeValue("Range");
        d->responseHeaders.addValue("Range", range);
    }
};

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

    
    const QnVideoResourceLayout* videoLayout = d->mediaRes->getVideoLayout(d->liveDpHi);
    const QnResourceAudioLayout* audioLayout = d->mediaRes->getAudioLayout(d->liveDpHi);
    int numVideo = videoLayout ? videoLayout->numberOfChannels() : 1;
    int numAudio = audioLayout ? audioLayout->numberOfChannels() : 0;

    addResponseRangeHeader();

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

/*
QnAbstractMediaStreamDataProvider* QnRtspConnectionProcessor::getLiveDp()
{
    Q_D(QnRtspConnectionProcessor);
    if (d->quality == MEDIA_Quality_High || d->liveDpLow == 0 || d->liveDpLow->onPause())
        return d->liveDpHi;
    else
        return d->liveDpLow;
}
*/

bool QnRtspConnectionProcessor::isSecondaryLiveDPSupported() const
{
    Q_D(const QnRtspConnectionProcessor);
    return d->liveDpLow && !d->liveDpLow->onPause();
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

    QnAbstractMediaStreamDataProvider* currentDP = d->liveMode ? d->liveDpHi : d->archiveDP;
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
    
    //if (d->archiveDP)
    //    d->archiveDP->setSingleShotMode(true);

    //d->playTime += d->rtspScale * d->playTimer.elapsed()*1000;
    //d->rtspScale = 0;

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
    return d->dataProcessor->getDisplayedTime();
}

void QnRtspConnectionProcessor::extractNptTime(const QString& strValue, qint64* dst)
{
    Q_D(QnRtspConnectionProcessor);
    d->liveMode = d->rtspScale >= 0 && strValue == "now";
    if (strValue == "now")
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
    QnVideoCamera* camera = qnCameraPool->getVideoCamera(d->mediaRes);
	if (camera)	
	{
		camera->inUse(d);
		if (!d->liveDpHi) {
			d->liveDpHi = camera->getLiveReader(QnResource::Role_LiveVideo);
			if (d->liveDpHi)
				d->liveDpHi->start();
		}
		if (!d->liveDpLow)  {
			d->liveDpLow = camera->getLiveReader(QnResource::Role_SecondaryLiveVideo);
			if (d->liveDpLow)
				d->liveDpLow->start();
		}
	}
	if (!d->archiveDP) 
		d->archiveDP = dynamic_cast<QnArchiveStreamReader*> (d->mediaRes->createDataProvider(QnResource::Role_Archive));
}

void QnRtspConnectionProcessor::connectToLiveDataProviders()
{
    Q_D(QnRtspConnectionProcessor);
    d->liveDpHi->addDataProcessor(d->dataProcessor);
    if (d->liveDpLow)
        d->liveDpLow->addDataProcessor(d->dataProcessor);

    d->dataProcessor->setLiveQuality(d->quality);
    d->dataProcessor->setLiveMarker(d->lastPlayCSeq);
}

void QnRtspConnectionProcessor::checkQuality()
{
    Q_D(QnRtspConnectionProcessor);
    if (d->quality == MEDIA_Quality_Low)
    {
        if (d->liveDpLow == 0) {
            d->quality = MEDIA_Quality_High;
            qWarning() << "Low quality not supported for camera" << d->mediaRes->getUniqueId();
        }
        else if (d->liveDpLow->onPause()) {
            d->quality = MEDIA_Quality_High;
            qWarning() << "Primary stream has big fps for camera" << d->mediaRes->getUniqueId() << ". Secondary stream is disabled.";
        }
    }
}

int QnRtspConnectionProcessor::composePlay()
{

    Q_D(QnRtspConnectionProcessor);
    if (d->mediaRes == 0)
        return CODE_NOT_FOUND;
    createDataProvider();
    checkQuality();

    d->lastPlayCSeq = d->requestHeaders.value("CSeq").toInt();

    if (!d->dataProcessor) {
        d->dataProcessor = new QnRtspDataConsumer(this);
        d->dataProcessor->pauseNetwork();
    }
    else 
        d->dataProcessor->clearUnprocessedData();

    if (d->liveMode) {
        if (d->archiveDP)
            d->archiveDP->stop();
    }
    else
    {
        if (d->liveDpHi)
            d->liveDpHi->removeDataProcessor(d->dataProcessor);
        if (d->liveDpLow)
            d->liveDpLow->removeDataProcessor(d->dataProcessor);
    }

    QnAbstractMediaStreamDataProvider* currentDP = d->liveMode ? d->liveDpHi : d->archiveDP;
    if (!currentDP)
        return CODE_NOT_FOUND;


	QnResource::Status status = getResource()->getStatus();

    d->dataProcessor->setLiveMode(d->liveMode);
    if (d->liveMode) {
        d->dataProcessor->lockDataQueue();
    }
    else {
        d->archiveDP->addDataProcessor(d->dataProcessor);
    }
    
    //QnArchiveStreamReader* archiveProvider = dynamic_cast<QnArchiveStreamReader*> (d->dataProvider);
    if (d->liveMode) 
    {
        int copySize = 0;
        if (status == QnResource::Online || status == QnResource::Recording) {
            copySize = d->dataProcessor->copyLastGopFromCamera(d->quality == MEDIA_Quality_High, 0);
        }

        if (copySize == 0) {
            // no data from the camera. Insert several empty packets to inform client about it
            for (int i = 0; i < 3; ++i)
            {
                QnEmptyMediaDataPtr emptyData(new QnEmptyMediaData());
                emptyData->flags |= QnAbstractMediaData::MediaFlags_LIVE;
                if (i == 0)
                    emptyData->flags |= QnAbstractMediaData::MediaFlags_BOF;
                emptyData->timestamp = DATETIME_NOW;
                emptyData->opaque = d->lastPlayCSeq;

                d->dataProcessor->addData(emptyData);
            }
        }

        d->dataProcessor->unlockDataQueue();
        d->dataProcessor->setWaitCSeq(d->startTime, 0); // ignore rest packets before new position
        connectToLiveDataProviders();
    }
    else if (d->archiveDP) 
    {
        QnServerArchiveDelegate* serverArchive = dynamic_cast<QnServerArchiveDelegate*>(d->archiveDP->getArchiveDelegate());
        if (serverArchive) 
        {
            /*
            QnTimePeriodList motionPeriods;
            QRegion region;
            QString motionRegion = d->requestHeaders.value("x-motion-region").toUtf8();
            if (motionRegion.size() > 0)
            {
                QBuffer buffer;
                buffer.open(QIODevice::ReadWrite);
                QDataStream stream(&buffer);
                stream << QByteArray::fromBase64(motionRegion.toUtf8());
                buffer.seek(0);
                stream >> region;
                serverArchive->setMotionRegion(region);
            }
            */
            QString sendMotion = d->requestHeaders.value("x-send-motion");
            if (!sendMotion.isNull()) {
                serverArchive->setSendMotion(sendMotion == "1" || sendMotion == "true");
            }
        }

        d->archiveDP->lock();
        d->archiveDP->setSpeed(d->rtspScale);
        d->archiveDP->setQuality(d->quality, d->qualityFastSwitch);
        if (!d->requestHeaders.value("Range").isNull())
        {
            d->dataProcessor->setSingleShotMode(d->startTime != DATETIME_NOW && d->startTime == d->endTime);
            d->dataProcessor->setWaitCSeq(d->startTime, d->lastPlayCSeq); // ignore rest packets before new position
            bool findIFrame = d->requestHeaders.value("x-no-find-iframe").isNull();
            d->archiveDP->jumpWithMarker(d->startTime, findIFrame, d->lastPlayCSeq);
        }
        else {
            d->archiveDP->setMarker(d->lastPlayCSeq);
        }
        d->archiveDP->unlock();

    }
    addResponseRangeHeader();

    d->dataProcessor->start();

    if (d->liveMode) {
        if (d->liveDpHi)
            d->liveDpHi->start();
        if (d->liveDpLow)
            d->liveDpLow->start();
    }
    else {
        if (d->archiveDP)
            d->archiveDP->start();
    }
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
    Q_D(QnRtspConnectionProcessor);

    createDataProvider();

    QList<QByteArray> parameters = d->requestBody.split('\n');
    foreach(const QByteArray& parameter, parameters)
    {
        if (parameter.trimmed().toLower().startsWith("x-media-quality"))
        {
            QList<QByteArray> vals = parameter.split(':');
            if (vals.size() < 2)
                return CODE_INVALID_PARAMETER;
            d->quality = vals[1].trimmed() == "low" ? MEDIA_Quality_Low : MEDIA_Quality_High;

            checkQuality();
            d->qualityFastSwitch = false;

            if (d->liveMode)
            {
                d->dataProcessor->lockDataQueue();

                connectToLiveDataProviders();

                qint64 time = d->dataProcessor->lastQueuedTime();
                d->dataProcessor->copyLastGopFromCamera(d->quality == MEDIA_Quality_High, time); // for fast quality switching

                // set "main" dataProvider. RTSP data consumer is going to unsubscribe from other dataProvider
                // then it will be possible (I frame received)

                d->dataProcessor->unlockDataQueue();
            }
            d->archiveDP->setQuality(d->quality, d->qualityFastSwitch);
            return CODE_OK;
        }
    }

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
            addResponseRangeHeader();
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
            if (isFullMessage(d->clientRequest))
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
