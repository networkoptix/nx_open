#include "video_camera.h"
#include "core/dataprovider/media_streamdataprovider.h"
#include "plugins/resources/archive/abstract_archive_stream_reader.h"
#include "ui/style/skin.h"
#include "core/resource/security_cam_resource.h"
#include "device_plugins/archive/rtsp/rtsp_client_archive_delegate.h"

QnVideoCamera::QnVideoCamera(QnMediaResourcePtr resource, QnAbstractMediaStreamDataProvider* reader) :
    m_resource(resource),
    m_camdispay(resource),
    m_reader(reader),
    m_extTimeSrc(NULL),
    m_isVisible(true),
    m_exportRecorder(0),
    m_exportReader(0),
    m_progressOffset(0)
{
    if (m_resource)
        cl_log.log(QLatin1String("Creating camera for "), m_resource->toString(), cl_logDEBUG1);
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
        }
    }    

}

QnVideoCamera::~QnVideoCamera()
{
    if (m_resource)
        cl_log.log(QLatin1String("Destroy camera for "), m_resource->toString(), cl_logDEBUG1);

    stopDisplay();
    delete m_reader;
}

QnMediaResourcePtr QnVideoCamera::resource() {
    return m_resource;
}

qint64 QnVideoCamera::getCurrentTime() const
{
    if (m_extTimeSrc && m_extTimeSrc->isEnabled())
        return m_extTimeSrc->getDisplayedTime();
    else
        return m_camdispay.getDisplayedTime();
}

/*
void QnVideoCamera::streamJump(qint64 time)
{
    m_camdispay.jump(time);
}
*/

void QnVideoCamera::startDisplay()
{
    CL_LOG(cl_logDEBUG1) cl_log.log(QLatin1String("QnVideoCamera::startDisplay "), m_resource->getUniqueId(), cl_logDEBUG1);

    m_camdispay.start();
    //m_reader->start(QThread::HighestPriority);
    m_reader->start(QThread::HighPriority);
}

void QnVideoCamera::stopDisplay()
{
    //CL_LOG(cl_logDEBUG1) cl_log.log(QLatin1String("QnVideoCamera::stopDisplay"), m_resource->getUniqueId(), cl_logDEBUG1);
    //CL_LOG(cl_logDEBUG1) cl_log.log(QLatin1String("QnVideoCamera::stopDisplay reader is about to pleases stop "), QString::number((long)m_reader,16), cl_logDEBUG1);

    m_reader->stop();
    m_camdispay.stop();
    m_camdispay.clearUnprocessedData();
}

void QnVideoCamera::beforeStopDisplay()
{
    m_reader->pleaseStop();
    m_camdispay.pleaseStop();
}

QnResourcePtr QnVideoCamera::getDevice() const
{
    return m_resource;
}

QnAbstractStreamDataProvider* QnVideoCamera::getStreamreader()
{
    return m_reader;
}

QnCamDisplay* QnVideoCamera::getCamDisplay()
{
    return &m_camdispay;
}

const QnStatistics* QnVideoCamera::getStatistics(int channel)
{
    return m_reader->getStatistics(channel);
}

void QnVideoCamera::setLightCPUMode(QnAbstractVideoDecoder::DecodeMode val)
{
    m_camdispay.setLightCPUMode(val);
}

void QnVideoCamera::setQuality(QnStreamQuality q, bool increase)
{
    Q_UNUSED(q)
    Q_UNUSED(increase)
    /*
    if (increase && m_reader->getQuality() >= q)
        return;

    if (!increase && m_reader->getQuality() <= q)
        return;

    if (isRecording())
        m_reader->setQuality(QnQualityHighest);
    else
        m_reader->setQuality(q);
        */
}

void QnVideoCamera::exportMediaPeriodToFile(qint64 startTime, qint64 endTime, const QString& fileName, const QString& format, QnStorageResourcePtr storage, 
                                            QnStreamRecorder::Role role, int timeOffsetMs)
{
    if (startTime > endTime)
        qSwap(startTime, endTime);

    if (m_exportRecorder == 0)
    {
        QnAbstractStreamDataProvider* tmpReader = m_resource->createDataProvider(QnResource::Role_Default);
        m_exportReader = dynamic_cast<QnAbstractArchiveReader*> (tmpReader);
        if (m_exportReader == 0)
        {
            delete tmpReader;
            emit recordingFailed(tr("Invalid resource type for data export."));
            return;
        }
        m_exportReader->setCycleMode(false);
        QnRtspClientArchiveDelegate* rtspClient = dynamic_cast<QnRtspClientArchiveDelegate*> (m_exportReader->getArchiveDelegate());
        if (rtspClient) {
            // 'slow' open mode. send DESCRIBE and SETUP to server.
            // it is required for av_streams in output file - we should know all codec context imediatly
            rtspClient->setResource(m_resource);
            rtspClient->setPlayNowModeAllowed(false); 
        }
        

        m_exportRecorder = new QnStreamRecorder(m_resource);
        m_exportRecorder->disconnect(this);
        if (storage)
            m_exportRecorder->setStorage(storage);
        connect(m_exportRecorder, SIGNAL(recordingFailed(QString)), this, SLOT(onExportFailed(QString)));
        connect(m_exportRecorder, SIGNAL(recordingFinished(QString)), this, SLOT(onExportFinished(QString)));
        connect(m_exportRecorder, SIGNAL(recordingProgress(int)), this, SLOT(at_exportProgress(int)));
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
    m_exportRecorder->setOnScreenDateOffset(timeOffsetMs);
    m_exportRecorder->setContainer(format);

#ifndef _DEBUG
    if (qSharedPointerDynamicCast<QnSecurityCamResource>(m_resource))
#endif
    {
        m_exportRecorder->setNeedCalcSignature(true);
        m_exportRecorder->setSignLogo(qnSkin->pixmap("logo_1920_1080.png"));
    }

    m_exportReader->addDataProcessor(m_exportRecorder);
    m_exportReader->jumpTo(startTime, startTime);

    m_exportReader->start();
    m_exportRecorder->start();
}

void QnVideoCamera::at_exportProgress(int value)
{
    emit exportProgress(value + m_progressOffset);
}

void QnVideoCamera::onExportFinished(QString fileName)
{
    stopExport();
    emit exportFinished(fileName);
}

void QnVideoCamera::onExportFailed(QString fileName)
{
    stopExport();
    emit exportFailed(fileName);
}

void QnVideoCamera::stopExport()
{
    if (m_exportReader)
        m_exportReader->stop();
    if (m_exportRecorder)
        m_exportRecorder->stop();
    QMutexLocker lock(&m_exportMutex);
    delete m_exportReader;
    delete m_exportRecorder;
    m_exportReader = 0;
    m_exportRecorder = 0;
}

void QnVideoCamera::setResource(QnMediaResourcePtr resource)
{
    m_resource = resource;
}

void QnVideoCamera::setExportProgressOffset(int value)
{
    m_progressOffset = value;
}

int QnVideoCamera::getExportProgressOffset() const
{
    return m_progressOffset;
}

void QnVideoCamera::setMotionIODevice(QSharedPointer<QBuffer> value, int channel)
{
    m_motionFileList[channel] = value;
}

QString QnVideoCamera::exportedFileName() const
{
    QMutexLocker lock(&m_exportMutex);
    if (m_exportRecorder)
        return m_exportRecorder->fixedFileName();
    else
        return QString();
}
