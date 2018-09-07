#include "client_video_camera.h"

#include <client_core/client_core_module.h>

#include <nx/utils/log/log.h>

#include <nx/streaming/abstract_media_stream_data_provider.h>
#include <nx/streaming/rtsp_client_archive_delegate.h>
#include <nx/streaming/archive_stream_reader.h>

#include <core/resource/media_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/security_cam_resource.h>

#include <nx/streaming/rtsp_client_archive_delegate.h>
#include <nx/streaming/archive_stream_reader.h>
#include <nx/network/http/custom_headers.h>
#include <core/dataprovider/data_provider_factory.h>

#include <recording/time_period.h>
#include <core/resource/avi/thumbnails_stream_reader.h>
#include <core/resource/avi/avi_archive_delegate.h>
#include <utils/common/util.h>

#include <media/filters/h264_mp4_to_annexb.h>

QnClientVideoCamera::QnClientVideoCamera(const QnMediaResourcePtr &resource, QnAbstractMediaStreamDataProvider* reader) :
    base_type(nullptr),
    m_resource(resource),
    m_camdispay(resource, dynamic_cast<QnArchiveStreamReader*>(reader)),
    m_reader(reader),
    m_exportRecorder(nullptr),
    m_exportReader(nullptr),
    m_displayStarted(false)
{
    if (m_reader)
    {
        m_reader->addDataProcessor(&m_camdispay);

        if (const auto archiveReader =
            dynamic_cast<QnAbstractArchiveStreamReader*>(m_reader.data()))
        {
            connect(archiveReader, &QnAbstractArchiveStreamReader::streamAboutToBePaused,
                &m_camdispay, &QnCamDisplay::onReaderPaused, Qt::DirectConnection);
            connect(archiveReader, &QnAbstractArchiveStreamReader::streamAboutToBeResumed,
                &m_camdispay, &QnCamDisplay::onReaderResumed, Qt::DirectConnection);
            connect(archiveReader, &QnAbstractArchiveStreamReader::prevFrameOccured,
                &m_camdispay, &QnCamDisplay::onPrevFrameOccured, Qt::DirectConnection);
            connect(archiveReader, &QnAbstractArchiveStreamReader::nextFrameOccured,
                &m_camdispay, &QnCamDisplay::onNextFrameOccured, Qt::DirectConnection);
            connect(archiveReader, &QnAbstractArchiveStreamReader::slowSourceHint,
                &m_camdispay, &QnCamDisplay::onSlowSourceHint, Qt::DirectConnection);
            connect(archiveReader, &QnAbstractArchiveStreamReader::beforeJump,
                &m_camdispay, &QnCamDisplay::onBeforeJump, Qt::DirectConnection);
            connect(archiveReader, &QnAbstractArchiveStreamReader::jumpOccured,
                &m_camdispay, &QnCamDisplay::onJumpOccured, Qt::DirectConnection);
            connect(archiveReader, &QnAbstractArchiveStreamReader::jumpCanceled,
                &m_camdispay, &QnCamDisplay::onJumpCanceled, Qt::DirectConnection);
            connect(archiveReader, &QnAbstractArchiveStreamReader::skipFramesTo,
                &m_camdispay, &QnCamDisplay::onSkippingFrames, Qt::DirectConnection);
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

/*
void QnClientVideoCamera::streamJump(qint64 time)
{
    m_camdispay.jump(time);
}
*/

void QnClientVideoCamera::startDisplay()
{
    NX_DEBUG(this, lm("startDisplay %1").arg(m_resource->toResource()->getUniqueId()));

    m_camdispay.start();
    if (m_reader)
        m_reader->start(QThread::HighPriority);
    m_displayStarted = true;
}

void QnClientVideoCamera::stopDisplay()
{
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

const QnMediaStreamStatistics* QnClientVideoCamera::getStatistics(int channel)
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
    const  QString& fileName,
    const QString& format,
    QnStorageResourcePtr storage,
    StreamRecorderRole role,
    qint64 serverTimeZoneMs)
{
    qint64 startTimeUs = timePeriod.startTimeMs * 1000ll;
    NX_ASSERT(timePeriod.durationMs > 0, Q_FUNC_INFO, "Invalid time period, possibly LIVE is exported");
    qint64 endTimeUs = timePeriod.durationMs > 0
        ? timePeriod.endTimeMs() * 1000ll
        : DATETIME_NOW;

    QnMutexLocker lock( &m_exportMutex );
    if (!m_exportRecorder)
    {

        auto tmpReader = qnClientCoreModule->dataProviderFactory()->createDataProvider(
            m_resource->toResourcePtr());
        QnAbstractArchiveStreamReader* archiveReader = dynamic_cast<QnAbstractArchiveStreamReader*> (tmpReader);
        if (!archiveReader)
        {
            delete tmpReader;

            const auto errorStruct = StreamRecorderErrorStruct(
                StreamRecorderError::invalidResourceType, QnStorageResourcePtr());
            emit exportFinished(errorStruct, fileName);
            return;
        }
        archiveReader->setCycleMode(false);
        if (role == StreamRecorderRole::fileExport)
        {
            archiveReader->setQuality(MEDIA_Quality_ForceHigh, true); // for 'mkv' and 'avi' files
            // Additing filtering is required in case of.AVI export.
                archiveReader->addMediaFilter(std::make_shared<H264Mp4ToAnnexB>());
        }

        QnRtspClientArchiveDelegate* rtspClient = dynamic_cast<QnRtspClientArchiveDelegate*> (archiveReader->getArchiveDelegate());
        if (rtspClient) {
            // 'slow' open mode. send DESCRIBE and SETUP to server.
            // it is required for av_streams in output file - we should know all codec context immediately
            QnVirtualCameraResourcePtr camera = m_resource->toResourcePtr().dynamicCast<QnVirtualCameraResource>();
            rtspClient->setCamera(camera);
            rtspClient->setPlayNowModeAllowed(false);
            rtspClient->setAdditionalAttribute(Qn::EC2_MEDIA_ROLE, "export");
        }

        m_exportReader = archiveReader;


        connect(m_exportReader, &QnAbstractArchiveStreamReader::finished, this,
            [this]()
            {
                {
                    QnMutexLocker lock(&m_exportMutex);
                    m_exportReader.clear();
                }

                sender()->deleteLater();
            });

        m_exportRecorder = new QnStreamRecorder(m_resource->toResourcePtr());
        connect(m_exportRecorder, &QnStreamRecorder::finished, this,
            [this]
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

        connect(m_exportRecorder,   &QnStreamRecorder::recordingFinished, this,   &QnClientVideoCamera::stopExport);
        connect(m_exportRecorder,   &QnStreamRecorder::recordingProgress, this,   &QnClientVideoCamera::exportProgress);
        connect(m_exportRecorder,   &QnStreamRecorder::recordingFinished, this,   &QnClientVideoCamera::exportFinished);

        if (fileName.toLower().endsWith(QLatin1String(".avi")))
        {
            m_exportRecorder->setAudioCodec(AV_CODEC_ID_MP2); // transcode audio to MP2
        }
    }
    QnAbstractArchiveStreamReader* archiveReader = dynamic_cast<QnAbstractArchiveStreamReader*> (m_exportReader.data());

    if (m_motionFileList[0] && archiveReader)
    {
        using namespace nx::vms::api;
        auto filter = archiveReader->streamDataFilter();
        filter.setFlag(StreamDataFilter::motion, true);
        filter.setFlag(StreamDataFilter::media, true);
        archiveReader->setStreamDataFilter(filter);
        m_exportRecorder->setMotionFileList(m_motionFileList);
    }
    // TODO: add analytics objects to an export file as well.

    m_exportRecorder->clearUnprocessedData();
    m_exportRecorder->setProgressBounds(startTimeUs, endTimeUs);

    if (storage)
        m_exportRecorder->addRecordingContext(fileName, storage);
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

void QnClientVideoCamera::stopExport()
{
    if (m_exportReader)
    {
        if (m_exportRecorder)
            m_exportReader->removeDataProcessor(m_exportRecorder);
        m_exportReader->pleaseStop();  // it will be deleted in finished() signal handle
    }
    if (m_exportRecorder)
    {
        // clean signature flag; in other case file will be recreated on writing finish
        // TODO: #vasilenko get rid of this magic
        m_exportRecorder->setNeedCalcSignature(false);

        connect(m_exportRecorder, &QnStreamRecorder::finished, this, &QnClientVideoCamera::exportStopped);
        m_exportRecorder->pleaseStop(); // it will be deleted in finished() signal handle
    }
    QnMutexLocker lock(&m_exportMutex);
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
