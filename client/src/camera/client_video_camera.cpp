#include "client_video_camera.h"

#include <utils/common/log.h>

#include <core/dataprovider/media_streamdataprovider.h>
#include <core/resource/media_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/security_cam_resource.h>

#include <plugins/resource/archive/rtsp_client_archive_delegate.h>
#include <plugins/resource/archive/archive_stream_reader.h>
#include "http/custom_headers.h"

#include <recording/time_period.h>
#include <plugins/resource/avi/thumbnails_stream_reader.h>
#include <plugins/resource/avi/avi_archive_delegate.h>

// input video with steps between frames in timeStepUsec translated to output videi with 30 fps (kOutputDeltaUsec between frames)

class QnTimeLapseRecorder: public QnStreamRecorder
{
    static const qint64 kOutputDeltaUsec = 1000000ll / 30; //< 30 fps

public:
    QnTimeLapseRecorder(const QnResourcePtr& resource, qint64 /*timeStepUsec */):
        QnStreamRecorder(resource),
        m_currentTimeUsec(0)
    {

    }

    virtual ~QnTimeLapseRecorder()
    {
        stop();
    }

protected:
    virtual qint64 getPacketTimeUsec(const QnConstAbstractMediaDataPtr& md) override
    {
        qint64 result = m_currentTimeUsec;
        m_currentTimeUsec += kOutputDeltaUsec;
        return result;
    }

    virtual bool isUtcOffsetAllowed() const override { return false; }
private:
    qint64 m_currentTimeUsec;
};


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

QnClientVideoCamera::QnClientVideoCamera(const QnMediaResourcePtr &resource, QnAbstractMediaStreamDataProvider* reader) :
    base_type(nullptr),
    m_resource(resource),
    m_camdispay(resource, dynamic_cast<QnArchiveStreamReader*>(reader)),
    m_reader(reader),
    m_extTimeSrc(NULL),
    m_exportRecorder(nullptr),
    m_exportReader(nullptr),
    m_displayStarted(false)
{
    if (m_reader) {
        m_reader->addDataProcessor(&m_camdispay);
        if (QnAbstractArchiveReader* archiveReader = dynamic_cast<QnAbstractArchiveReader*>(m_reader.data())) {
            connect(archiveReader, &QnAbstractArchiveReader::streamPaused,       &m_camdispay, &QnCamDisplay::onReaderPaused,        Qt::DirectConnection);
            connect(archiveReader, &QnAbstractArchiveReader::streamResumed,      &m_camdispay, &QnCamDisplay::onReaderResumed,       Qt::DirectConnection);
            connect(archiveReader, &QnAbstractArchiveReader::prevFrameOccured,   &m_camdispay, &QnCamDisplay::onPrevFrameOccured,    Qt::DirectConnection);
            connect(archiveReader, &QnAbstractArchiveReader::nextFrameOccured,   &m_camdispay, &QnCamDisplay::onNextFrameOccured,    Qt::DirectConnection);
            connect(archiveReader, &QnAbstractArchiveReader::slowSourceHint,     &m_camdispay, &QnCamDisplay::onSlowSourceHint,      Qt::DirectConnection);
            connect(archiveReader, &QnAbstractArchiveReader::beforeJump,         &m_camdispay, &QnCamDisplay::onBeforeJump,          Qt::DirectConnection);
            connect(archiveReader, &QnAbstractArchiveReader::jumpOccured,        &m_camdispay, &QnCamDisplay::onJumpOccured,         Qt::DirectConnection);
            connect(archiveReader, &QnAbstractArchiveReader::jumpCanceled,       &m_camdispay, &QnCamDisplay::onJumpCanceled,        Qt::DirectConnection);
            connect(archiveReader, &QnAbstractArchiveReader::skipFramesTo,       &m_camdispay, &QnCamDisplay::onSkippingFrames,      Qt::DirectConnection);
        }
    }

}

QnClientVideoCamera::~QnClientVideoCamera()
{
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

void QnClientVideoCamera::exportMediaPeriodToFile(const QnTimePeriod &timePeriod,
												  const  QString& fileName, const QString& format,
                                            QnStorageResourcePtr storage,
                                            QnStreamRecorder::Role role,
                                            qint64 serverTimeZoneMs,
                                            qint64 timelapseFrameStepMs,
                                            QnImageFilterHelper transcodeParams)
{
    qint64 timelapseFrameStepUs = timelapseFrameStepMs * 1000;
    qint64 startTimeUs = timePeriod.startTimeMs * 1000ll;
    Q_ASSERT_X(timePeriod.durationMs > 0, Q_FUNC_INFO, "Invalid time period, possibly LIVE is exported");
    qint64 endTimeUs = timePeriod.durationMs > 0
        ? timePeriod.endTimeMs() * 1000ll
        : DATETIME_NOW;

    QnMutexLocker lock( &m_exportMutex );
    if (!m_exportRecorder)
    {
        if (m_resource->toResource()->hasFlags(Qn::local) && timelapseFrameStepUs > 0)
        {
            auto thumbnailsReader = new QnThumbnailsStreamReader(m_resource->toResourcePtr(), new QnAviArchiveDelegate());
            thumbnailsReader->setRange(startTimeUs, endTimeUs, timelapseFrameStepUs, 0);
            m_exportReader = thumbnailsReader;
        }
        else
        {
            auto tmpReader = m_resource->toResource()->createDataProvider(Qn::CR_Default);
            QnAbstractArchiveReader* archiveReader = dynamic_cast<QnAbstractArchiveReader*> (tmpReader);
            if (!archiveReader)
            {
                delete tmpReader;
                emit exportFinished(
                    QnStreamRecorder::ErrorStruct(
                    InvalidResourceType,
                    QnStorageResourcePtr()
                    ),
                    fileName
                    );
                return;
            }
            archiveReader->setCycleMode(false);

            QnRtspClientArchiveDelegate* rtspClient = dynamic_cast<QnRtspClientArchiveDelegate*> (archiveReader->getArchiveDelegate());
            if (rtspClient) {
                // 'slow' open mode. send DESCRIBE and SETUP to server.
                // it is required for av_streams in output file - we should know all codec context immediately
                QnVirtualCameraResourcePtr camera = m_resource->toResourcePtr().dynamicCast<QnVirtualCameraResource>();
                rtspClient->setCamera(camera);
                rtspClient->setPlayNowModeAllowed(false);
                rtspClient->setAdditionalAttribute(Qn::EC2_MEDIA_ROLE, "export");
                if (timelapseFrameStepUs > 0)
                    rtspClient->setRange(startTimeUs, endTimeUs, timelapseFrameStepUs);
            }

            if (role == QnStreamRecorder::Role_FileExport)
                archiveReader->setQuality(MEDIA_Quality_ForceHigh, true); // for 'mkv' and 'avi' files


            m_exportReader = archiveReader;
        }

        connect(m_exportReader, &QnAbstractArchiveReader::finished, this, [this]()
        {
            {
                QnMutexLocker lock(&m_exportMutex);
                m_exportReader.clear();
            }

            /* There is a possibility we have already cleared the smart pointer, e.g. in stopExport() method. */
            sender()->deleteLater();
        });

        if (timelapseFrameStepUs > 0)
            m_exportRecorder = new QnTimeLapseRecorder(m_resource->toResourcePtr(), timelapseFrameStepUs);
        else
            m_exportRecorder = new QnStreamRecorder(m_resource->toResourcePtr());


        connect(m_exportRecorder, &QnStreamRecorder::finished, this, [this]()
        {
            {
                QnMutexLocker lock(&m_exportMutex);
                if (m_exportReader && m_exportRecorder)
                    m_exportReader->removeDataProcessor(m_exportRecorder);
                m_exportRecorder.clear();
            }

            /* There is a possibility we have already cleared the smart pointer, e.g. in stopExport() method. */
            sender()->deleteLater();
        });


        m_exportRecorder->setExtraTranscodeParams(transcodeParams);

        connect(m_exportRecorder,   &QnStreamRecorder::recordingFinished, this,   &QnClientVideoCamera::stopExport);
        connect(m_exportRecorder,   &QnStreamRecorder::recordingProgress, this,   &QnClientVideoCamera::exportProgress);
        connect(m_exportRecorder,   &QnStreamRecorder::recordingFinished, this,   &QnClientVideoCamera::exportFinished);

        if (fileName.toLower().endsWith(QLatin1String(".avi")))
        {
            m_exportRecorder->setAudioCodec(CODEC_ID_MP3); // transcode audio to MP3
        }
    }
    QnAbstractArchiveReader* archiveReader = dynamic_cast<QnAbstractArchiveReader*> (m_exportRecorder.data());

    if (m_motionFileList[0] && archiveReader)
    {
        archiveReader->setSendMotion(true);
        m_exportRecorder->setMotionFileList(m_motionFileList);
    }

    m_exportRecorder->clearUnprocessedData();
    m_exportRecorder->setEofDateTime(endTimeUs);

    if (storage)
        m_exportRecorder->addRecordingContext(
            fileName,
            storage
        );
    else
        m_exportRecorder->addRecordingContext(fileName);

    m_exportRecorder->setRole(role);
    m_exportRecorder->setServerTimeZoneMs(serverTimeZoneMs);
    m_exportRecorder->setContainer(format);
    m_exportRecorder->setNeedCalcSignature(true);

    m_exportReader->addDataProcessor(m_exportRecorder);
    if (archiveReader)
        archiveReader->jumpTo(startTimeUs, startTimeUs);

    m_exportReader->start();
    m_exportRecorder->start();
}

void QnClientVideoCamera::stopExport() {
    if (m_exportReader) {
        if (m_exportRecorder)
            m_exportReader->removeDataProcessor(m_exportRecorder);
        m_exportReader->pleaseStop();  // it will be deleted in finished() signal handle
    }
    if (m_exportRecorder) {
        // clean signature flag; in other case file will be recreated on writing finish
        //TODO: #vasilenko get rid of this magic
        m_exportRecorder->setNeedCalcSignature(false);

        connect(m_exportRecorder, SIGNAL(finished()), this, SIGNAL(exportStopped()));
        m_exportRecorder->pleaseStop(); // it will be deleted in finished() signal handle
    }
    QnMutexLocker lock( &m_exportMutex );
    m_exportReader.clear();
    m_exportRecorder.clear();
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
    QnMutexLocker lock( &m_exportMutex );
    if (m_exportRecorder)
        return m_exportRecorder->fixedFileName();
    else
        return QString();
}
