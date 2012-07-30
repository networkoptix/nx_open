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
#include "core/dataprovider/live_stream_provider.h"
#include "core/resource/resource_fwd.h"
#include "core/resource/camera_resource.h"
#include "device_plugins/server_archive/thumbnails_stream_reader.h"

class QnTcpListener;

// ----------------------------- QnRtspConnectionProcessorPrivate ----------------------------

enum Mode {Mode_Live, Mode_Archive, Mode_ThumbNails};
static const int MAX_CAMERA_OPEN_TIME = 1000 * 5;

class QnRtspConnectionProcessor::QnRtspConnectionProcessorPrivate: public QnTCPConnectionProcessor::QnTCPConnectionProcessorPrivate
{
public:
    //enum State {State_Stopped, State_Paused, State_Playing, State_Rewind};

    QnRtspConnectionProcessorPrivate():
        QnTCPConnectionProcessor::QnTCPConnectionProcessorPrivate(),
        dataProcessor(0),
        startTime(0),
        endTime(0),
        rtspScale(1.0),
        liveMode(Mode_Live),
        gotLivePacket(false),
        lastPlayCSeq(0),
        quality(MEDIA_Quality_High),
        qualityFastSwitch(true),
        prevStartTime(AV_NOPTS_VALUE),
        prevEndTime(AV_NOPTS_VALUE),
        metadataChannelNum(7),
        audioEnabled(false)
    {
    }

    QnAbstractMediaStreamDataProviderPtr getCurrentDP()
    {
        if (liveMode == Mode_ThumbNails)
            return thumbnailsDP;
        else if (liveMode == Mode_Live)
            return liveDpHi;
        else 
            return archiveDP;
    }


    void deleteDP()
    {
        if (archiveDP)
            archiveDP->stop();
        if (thumbnailsDP)
            thumbnailsDP->stop();
        if (dataProcessor)
            dataProcessor->stop();

        if (liveDpHi)
            liveDpHi->removeDataProcessor(dataProcessor);
        if (liveDpLow)
            liveDpLow->removeDataProcessor(dataProcessor);
        if (archiveDP)
            archiveDP->removeDataProcessor(dataProcessor);
        if (thumbnailsDP)
            thumbnailsDP->removeDataProcessor(dataProcessor);
		archiveDP.clear();
        delete dataProcessor;
        dataProcessor = 0;

        QnVideoCamera* camera = qnCameraPool->getVideoCamera(mediaRes);
		if (camera)
			camera->notInUse(this);
    }

    ~QnRtspConnectionProcessorPrivate()
    {
        deleteDP();
    }

    QnAbstractMediaStreamDataProviderPtr liveDpHi;
    QnAbstractMediaStreamDataProviderPtr liveDpLow;
    QSharedPointer<QnArchiveStreamReader> archiveDP;
    QSharedPointer<QnThumbnailsStreamReader> thumbnailsDP;
    Mode liveMode;

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
    int metadataChannelNum;
    bool audioEnabled;
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

    if (!d->requestHeaders.value("x-media-step").isEmpty())
        d->liveMode = Mode_ThumbNails;
    else if (d->rtspScale >= 0 && d->startTime == DATETIME_NOW)
        d->liveMode = Mode_Live;
    else
        d->liveMode = Mode_Archive;


    if (d->mediaRes == 0)
    {
        QString resId = extractPath();
        if (resId.startsWith('/'))
            resId = resId.mid(1);
        QnResourcePtr resource = qnResPool->getResourceByUrl(resId);
        if (resource == 0) {
            resource = qnResPool->getNetResourceByPhysicalId(resId);
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

QString QnRtspConnectionProcessor::getRangeHeaderIfChanged()
{
    Q_D(QnRtspConnectionProcessor);
    QMutexLocker lock(&d->mutex);

    if (!d->archiveDP)
        return QString();

    qint64 endTime = d->archiveDP->endTime();
    if (QnRecordingManager::instance()->isCameraRecoring(d->mediaRes)) 
        endTime = DATETIME_NOW;

    if (d->archiveDP->startTime() != d->prevStartTime || endTime != d->prevEndTime)
    {
        return getRangeStr();
    }
    else {
        return QString();
    }
};

void QnRtspConnectionProcessor::sendResponse(int code)
{
    QnTCPConnectionProcessor::sendResponse("RTSP", code, "application/sdp");
}

int QnRtspConnectionProcessor::getMetadataTcpChannel() const
{
    Q_D(const QnRtspConnectionProcessor);
    return getAVTcpChannel(d->metadataChannelNum);
}

int QnRtspConnectionProcessor::getAVTcpChannel(int trackNum) const
{
    Q_D(const QnRtspConnectionProcessor);
    if (d->trackPorts.contains(trackNum))
        return d->trackPorts.value(trackNum).first;
    else
        return -1;
}

int QnRtspConnectionProcessor::numOfVideoChannels()
{
    Q_D(QnRtspConnectionProcessor);
    if (!d->mediaRes)
        return -1;
    QnAbstractMediaStreamDataProviderPtr currentDP = d->getCurrentDP();
    
    const QnVideoResourceLayout* layout = d->mediaRes->getVideoLayout(currentDP.data());
    return layout ? layout->numberOfChannels() : -1;
}

QString QnRtspConnectionProcessor::getRangeStr()
{
    Q_D(QnRtspConnectionProcessor);
    QString range;
    if (d->archiveDP) 
    {
        range = "npt=";
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
    }
    return range;
};

void QnRtspConnectionProcessor::addResponseRangeHeader()
{
    Q_D(QnRtspConnectionProcessor);
    QString range = getRangeStr();
    if (!range.isEmpty())
    {
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

    
    const QnVideoResourceLayout* videoLayout = d->mediaRes->getVideoLayout(d->liveDpHi.data());

    int numAudio = 0;
    QnVirtualCameraResourcePtr cameraResource = qSharedPointerDynamicCast<QnVirtualCameraResource>(d->mediaRes);
    if (cameraResource) {
        // avoid race condition if camera is starting now
        d->audioEnabled = cameraResource->isAudioEnabled();
        if (d->audioEnabled)
            numAudio = 1;
    }
    else {
        const QnResourceAudioLayout* audioLayout = d->mediaRes->getAudioLayout(d->liveDpHi.data());
        if (audioLayout)
            numAudio = audioLayout->numberOfChannels();
    }

    int numVideo = videoLayout ? videoLayout->numberOfChannels() : 1;

    addResponseRangeHeader();

    int i = 0;
    for (; i < numVideo + numAudio; ++i)
    {
        sdp << "m=" << (i < numVideo ? "video " : "audio ") << i << " RTP/AVP " << RTP_FFMPEG_GENERIC_CODE << ENDL;
        sdp << "a=control:trackID=" << i << ENDL;
        sdp << "a=rtpmap:" << RTP_FFMPEG_GENERIC_CODE << ' ' << RTP_FFMPEG_GENERIC_STR << "/" << CLOCK_FREQUENCY << ENDL;
    }

    //d->metadataChannelNum = i;
    sdp << "m=metadata " << d->metadataChannelNum << " RTP/AVP " << RTP_METADATA_CODE << ENDL;
    sdp << "a=control:trackID=" << d->metadataChannelNum << ENDL;
    sdp << "a=rtpmap:" << RTP_METADATA_CODE << ' ' << RTP_METADATA_GENERIC_STR << "/" << CLOCK_FREQUENCY << ENDL;

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

    QnAbstractMediaStreamDataProviderPtr currentDP = d->getCurrentDP();
    
    const QnVideoResourceLayout* videoLayout = d->mediaRes->getVideoLayout(currentDP.data());
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
	if (d->dataProcessor)
		return d->dataProcessor->getDisplayedTime();
	else
		return AV_NOPTS_VALUE;
}

void QnRtspConnectionProcessor::extractNptTime(const QString& strValue, qint64* dst)
{
    Q_D(QnRtspConnectionProcessor);
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

void QnRtspConnectionProcessor::at_cameraUpdated()
{
    Q_D(QnRtspConnectionProcessor);
	QMutexLocker lock(&d->mutex);

    QnVirtualCameraResourcePtr cameraResource = qSharedPointerDynamicCast<QnVirtualCameraResource>(d->mediaRes);
    if (cameraResource) {
        if (cameraResource->isAudioEnabled() != d->audioEnabled) {
            m_needStop = true;
            d->socket->shutdown();
        }
    }
}

void QnRtspConnectionProcessor::at_cameraDisabledChanged(bool oldValue, bool newValue)
{
    Q_UNUSED(oldValue)
    Q_D(QnRtspConnectionProcessor);
	QMutexLocker lock(&d->mutex);
    if (newValue) {
        m_needStop = true;
        d->socket->shutdown();
    }
}

void QnRtspConnectionProcessor::createDataProvider()
{
    Q_D(QnRtspConnectionProcessor);
    QnVideoCamera* camera = qnCameraPool->getVideoCamera(d->mediaRes);
	if (camera)	
	{
		camera->inUse(d);
		if (!d->liveDpHi && !d->mediaRes->isDisabled()) {
			d->liveDpHi = camera->getLiveReader(QnResource::Role_LiveVideo);
            if (d->liveDpHi) {
                connect(d->liveDpHi->getResource().data(), SIGNAL(disabledChanged(bool, bool)), this, SLOT(at_cameraDisabledChanged(bool, bool)), Qt::DirectConnection);
                connect(d->liveDpHi->getResource().data(), SIGNAL(resourceChanged()), this, SLOT(at_cameraUpdated()), Qt::DirectConnection);
				d->liveDpHi->start();
            }
		}
		if (!d->liveDpLow && d->liveDpHi)
        {
            QnVirtualCameraResourcePtr cameraRes = qSharedPointerDynamicCast<QnVirtualCameraResource> (d->mediaRes);
            QSharedPointer<QnLiveStreamProvider> liveHiProvider = qSharedPointerDynamicCast<QnLiveStreamProvider> (d->liveDpHi);
            if (cameraRes && liveHiProvider && cameraRes->getMaxFps() - liveHiProvider->getFps() >= QnRecordingManager::MIN_SECONDARY_FPS)
            {
			    d->liveDpLow = camera->getLiveReader(QnResource::Role_SecondaryLiveVideo);
                if (d->liveDpLow)
                    d->liveDpLow->start();
            }
		}
	}
	if (!d->archiveDP) 
		d->archiveDP = QSharedPointer<QnArchiveStreamReader> (dynamic_cast<QnArchiveStreamReader*> (d->mediaRes->createDataProvider(QnResource::Role_Archive)));

    if (!d->thumbnailsDP) 
        d->thumbnailsDP = QSharedPointer<QnThumbnailsStreamReader>(new QnThumbnailsStreamReader(d->mediaRes));
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

    if (d->liveMode == Mode_Live) {
        if (d->archiveDP)
            d->archiveDP->stop();
        if (d->thumbnailsDP)
            d->thumbnailsDP->stop();
    }
    else
    {
        if (d->liveDpHi)
            d->liveDpHi->removeDataProcessor(d->dataProcessor);
        if (d->liveDpLow)
            d->liveDpLow->removeDataProcessor(d->dataProcessor);

        if (d->liveMode == Mode_Archive && d->archiveDP)
            d->archiveDP->stop();
        if (d->liveMode == Mode_ThumbNails && d->thumbnailsDP)
            d->thumbnailsDP->stop();
    }

    QnAbstractMediaStreamDataProviderPtr currentDP = d->getCurrentDP();

    if (!currentDP)
        return CODE_NOT_FOUND;


	QnResource::Status status = getResource()->getStatus();

    d->dataProcessor->setLiveMode(d->liveMode == Mode_Live);
    
    //QnArchiveStreamReader* archiveProvider = dynamic_cast<QnArchiveStreamReader*> (d->dataProvider);
    if (d->liveMode == Mode_Live) 
    {
        d->dataProcessor->lockDataQueue();

        int copySize = 0;
		if (!getResource()->isDisabled() && (status == QnResource::Online || status == QnResource::Recording)) {
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
    else if (d->liveMode == Mode_Archive && d->archiveDP) 
    {
        d->archiveDP->addDataProcessor(d->dataProcessor);

        QnServerArchiveDelegate* serverArchive = dynamic_cast<QnServerArchiveDelegate*>(d->archiveDP->getArchiveDelegate());
        if (serverArchive) 
        {
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
    else if (d->liveMode == Mode_ThumbNails && d->thumbnailsDP) 
    {
        d->thumbnailsDP->addDataProcessor(d->dataProcessor);
        d->thumbnailsDP->setRange(d->startTime, d->endTime, d->requestHeaders.value("x-media-step").toLongLong(), d->lastPlayCSeq);
    }

    addResponseRangeHeader();
    d->dataProcessor->start();

    if (currentDP) 
        currentDP->start();
    if (d->liveMode == Mode_Live && d->liveDpLow)
        d->liveDpLow->start();

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

            if (d->liveMode == Mode_Live)
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
            d->receiveBuffer.append((const char*) d->tcpReadBuffer, readed);
            int msgLen = isFullMessage(d->receiveBuffer);
            if (msgLen)
            {
                d->clientRequest = d->receiveBuffer.left(msgLen);
                d->receiveBuffer.remove(0, msgLen);
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
    d->liveMode = Mode_Live;
    composePlay();
}
