#include "client_video_camera.h"

#include "core/dataprovider/media_streamdataprovider.h"
#include "ui/style/skin.h"
#include "core/resource/security_cam_resource.h"
#include "device_plugins/archive/rtsp/rtsp_client_archive_delegate.h"
#include "plugins/resources/archive/archive_stream_reader.h"

QString QnClientVideoCamera::errorString(int errCode) {
    switch (errCode) {
    case NoError:
        return QString();
    case InvalidResourceType:
        return tr("Invalid resource type for data export.");
    default:
        break;
    }

    return QnStreamRecorder::errorString(errCode);
}

QnClientVideoCamera::QnClientVideoCamera(QnMediaResourcePtr resource, QnAbstractMediaStreamDataProvider* reader) :
    m_resource(resource),
    m_camdispay(resource, dynamic_cast<QnArchiveStreamReader*>(reader)),
    m_reader(reader),
    m_extTimeSrc(NULL),
    m_isVisible(true),
    m_exportRecorder(0),
    m_exportReader(0),
    m_displayStarted(false)
{
    if (m_resource)
        cl_log.log(QLatin1String("Creating camera for "), m_resource->toResource()->toString(), cl_logDEBUG1);
    if (m_reader) {
        m_reader->addDataProcessor(&m_camdispay);
        if (dynamic_cast<QnAbstractArchiveReader*>(m_reader)) {
            connect(m_reader, SIGNAL(streamPaused()), &m_camdispay, SLOT(onReaderPaused()), Qt::DirectConnection);
            connect(m_reader, SIGNAL(streamResumed()), &m_camdispay, SLOT(onReaderResumed()), Qt::DirectConnection);
            connect(m_reader, SIGNAL(prevFrameOccured()), &m_camdispay, SLOT(onPrevFrameOccured()), Qt::DirectConnection);
            connect(m_reader, SIGNAL(nextFrameOccured()), &m_camdispay, SLOT(onNextFrameOccured()), Qt::DirectConnection);

            connect(m_reader, SIGNAL(slowSourceHint()), &m_camdispay, SLOT(onSlowSourceHint()), Qt::DirectConnection);
            connect(m_reader, SIGNAL(beforeJump(qint64)), &m_camdispay, SLOT(onBeforeJump(qint64)), Qt::DirectConnection);
            connect(m_reader, SIGNAL(jumpOccured(qint64)), &m_camdispay, SLOT(onJumpOccured(qint64)), Qt::DirectConnection);
            connect(m_reader, SIGNAL(jumpCanceled(qint64)), &m_camdispay, SLOT(onJumpCanceled(qint64)), Qt::DirectConnection);
            connect(m_reader, SIGNAL(skipFramesTo(qint64)), &m_camdispay, SLOT(onSkippingFrames(qint64)), Qt::DirectConnection);
        }
    }    

}

QnClientVideoCamera::~QnClientVideoCamera()
{
    if (m_resource)
        cl_log.log(QLatin1String("Destroy camera for "), m_resource->toResource()->toString(), cl_logDEBUG1);

    stopDisplay();
    delete m_reader;
}

QnMediaResourcePtr QnClientVideoCamera::resource() {
    return m_resource;
}

qint64 QnClientVideoCamera::getCurrentTime() const
{
    if (m_extTimeSrc && m_extTimeSrc->isEnabled())
        return m_extTimeSrc->getDisplayedTime();
    else
        return m_camdispay.getDisplayedTime();
}

/*
void QnClientVideoCamera::streamJump(qint64 time)
{
    m_camdispay.jump(time);
}
*/

void QnClientVideoCamera::startDisplay()
{
    CL_LOG(cl_logDEBUG1) cl_log.log(QLatin1String("QnClientVideoCamera::startDisplay "), m_resource->toResource()->getUniqueId(), cl_logDEBUG1);

    m_camdispay.start();
    if (m_reader)
        m_reader->start(QThread::HighPriority);
    m_displayStarted = true;
}

void QnClientVideoCamera::stopDisplay()
{
    //CL_LOG(cl_logDEBUG1) cl_log.log(QLatin1String("QnClientVideoCamera::stopDisplay"), m_resource->getUniqueId(), cl_logDEBUG1);
    //CL_LOG(cl_logDEBUG1) cl_log.log(QLatin1String("QnClientVideoCamera::stopDisplay reader is about to pleases stop "), QString::number((long)m_reader,16), cl_logDEBUG1);
    m_displayStarted = false;
    if (m_reader)
        m_reader->stop();
    m_camdispay.stop();
    m_camdispay.clearUnprocessedData();
}

void QnClientVideoCamera::beforeStopDisplay()
{
    if (m_reader)
        m_reader->pleaseStop();
    m_camdispay.pleaseStop();
}

QnResourcePtr QnClientVideoCamera::getDevice() const
{
    return m_resource->toResourcePtr();
}

QnAbstractStreamDataProvider* QnClientVideoCamera::getStreamreader()
{
    return m_reader;
}

QnCamDisplay* QnClientVideoCamera::getCamDisplay()
{
    return &m_camdispay;
}

const QnStatistics* QnClientVideoCamera::getStatistics(int channel)
{
    if (m_reader)
        return m_reader->getStatistics(channel);
    return NULL;
}

void QnClientVideoCamera::setLightCPUMode(QnAbstractVideoDecoder::DecodeMode val)
{
    m_camdispay.setLightCPUMode(val);
}

void QnClientVideoCamera::exportMediaPeriodToFile(qint64 startTime, qint64 endTime, const
                                            QString& fileName, const QString& format, 
                                            QnStorageResourcePtr storage, 
                                            QnStreamRecorder::Role role, 
                                            Qn::Corner timestamps,
                                            qint64 timeOffsetMs, qint64 serverTimeZoneMs,
                                            QRectF srcRect,
                                            const ImageCorrectionParams& contrastParams,
                                            const QnItemDewarpingParams& itemDwarpingParams)
{
    if (startTime > endTime)
        qSwap(startTime, endTime);

    QMutexLocker lock(&m_exportMutex);
    if (m_exportRecorder == 0)
    {
        QnAbstractStreamDataProvider* tmpReader = m_resource->toResource()->createDataProvider(QnResource::Role_Default);
        m_exportReader = dynamic_cast<QnAbstractArchiveReader*> (tmpReader);
        if (!m_exportReader)
        {
            delete tmpReader;
            emit exportFinished(InvalidResourceType, fileName);
            return;
        }
        m_exportReader->setCycleMode(false);
        QnRtspClientArchiveDelegate* rtspClient = dynamic_cast<QnRtspClientArchiveDelegate*> (m_exportReader->getArchiveDelegate());
        if (rtspClient) {
            // 'slow' open mode. send DESCRIBE and SETUP to server.
            // it is required for av_streams in output file - we should know all codec context imediatly
            rtspClient->setResource(m_resource->toResourcePtr());
            rtspClient->setPlayNowModeAllowed(false); 
        }
        if (role == QnStreamRecorder::Role_FileExport)
            m_exportReader->setQuality(MEDIA_Quality_ForceHigh, true); // for 'mkv' and 'avi' files

        m_exportRecorder = new QnStreamRecorder(m_resource->toResourcePtr());
        if (storage)
            m_exportRecorder->setStorage(storage);
        m_exportRecorder->setSrcRect(srcRect);
        m_exportRecorder->setContrastParams(contrastParams);
        m_exportRecorder->setItemDewarpingParams(itemDwarpingParams);
        connect(m_exportRecorder,   &QnStreamRecorder::recordingFinished, this,   &QnClientVideoCamera::stopExport);
        connect(m_exportRecorder,   &QnStreamRecorder::recordingProgress, this,   &QnClientVideoCamera::exportProgress);
        connect(m_exportRecorder,   &QnStreamRecorder::recordingFinished, this,   &QnClientVideoCamera::exportFinished);

        if (fileName.toLower().endsWith(QLatin1String(".avi")))
        {
            m_exportRecorder->setAudioCodec(CODEC_ID_MP3); // transcode audio to MP3
        }
    }
    if (m_motionFileList[0]) {
        m_exportReader->setSendMotion(true);
        m_exportRecorder->setMotionFileList(m_motionFileList);
    }

    m_exportRecorder->clearUnprocessedData();
    m_exportRecorder->setEofDateTime(endTime);
    m_exportRecorder->setFileName(fileName);
    m_exportRecorder->setRole(role);
    m_exportRecorder->setTimestampCorner(timestamps);
    m_exportRecorder->setOnScreenDateOffset(timeOffsetMs);
    m_exportRecorder->setServerTimeZoneMs(serverTimeZoneMs);
    m_exportRecorder->setContainer(format);

    {
        m_exportRecorder->setNeedCalcSignature(true);
#ifdef SIGN_FRAME_ENABLE
        m_exportRecorder->setSignLogo(qnSkin->pixmap("logo_1920_1080.png").toImage());
#endif
    }

    m_exportReader->addDataProcessor(m_exportRecorder);
    m_exportReader->jumpTo(startTime, startTime);
    m_exportReader->start();
    m_exportRecorder->start();
}

void QnClientVideoCamera::stopExport() {
    if (m_exportReader) {
        if (m_exportRecorder)
            m_exportReader->removeDataProcessor(m_exportRecorder);
        connect(m_exportReader, SIGNAL(finished()), m_exportReader, SLOT(deleteLater()));
        m_exportReader->pleaseStop();
    }
    if (m_exportRecorder) {
        // clean signature flag; in other case file will be recreated on writing finish
        //TODO: #vasilenko get rid of this magic
        m_exportRecorder->setNeedCalcSignature(false);

        connect(m_exportRecorder, SIGNAL(finished()), m_exportRecorder, SLOT(deleteLater()));
        m_exportRecorder->pleaseStop();
    }
    QMutexLocker lock(&m_exportMutex);
    m_exportReader = 0;
    m_exportRecorder = 0;
}

void QnClientVideoCamera::setResource(QnMediaResourcePtr resource)
{
    m_resource = resource;
}

void QnClientVideoCamera::setMotionIODevice(QSharedPointer<QBuffer> value, int channel)
{
    m_motionFileList[channel] = value;
}

QSharedPointer<QBuffer> QnClientVideoCamera::motionIODevice(int channel) {
    return m_motionFileList[channel];
}

QString QnClientVideoCamera::exportedFileName() const
{
    QMutexLocker lock(&m_exportMutex);
    if (m_exportRecorder)
        return m_exportRecorder->fixedFileName();
    else
        return QString();
}
