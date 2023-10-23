// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client_video_camera.h"

#include <client_core/client_core_module.h>

#include <nx/utils/log/log.h>
#include <nx/utils/datetime.h>

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
#include <common/common_module.h>

#include <media/filters/h264_mp4_to_annexb.h>

#include <nx/vms/client/desktop/ini.h>
#include <utils/common/delayed.h>

QnClientVideoCamera::QnClientVideoCamera(const QnMediaResourcePtr &resource, QnAbstractMediaStreamDataProvider* reader):
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
            connect(archiveReader, &QnAbstractArchiveStreamReader::prevFrameOccurred,
                &m_camdispay, &QnCamDisplay::onPrevFrameOccurred, Qt::DirectConnection);
            connect(archiveReader, &QnAbstractArchiveStreamReader::nextFrameOccurred,
                &m_camdispay, &QnCamDisplay::onNextFrameOccurred, Qt::DirectConnection);
            connect(archiveReader, &QnAbstractArchiveStreamReader::slowSourceHint,
                &m_camdispay, &QnCamDisplay::onSlowSourceHint, Qt::DirectConnection);
            connect(archiveReader, &QnAbstractArchiveStreamReader::beforeJump,
                &m_camdispay, &QnCamDisplay::onBeforeJump, Qt::DirectConnection);
            connect(archiveReader, &QnAbstractArchiveStreamReader::jumpOccurred,
                &m_camdispay, &QnCamDisplay::onJumpOccurred, Qt::DirectConnection);
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
    NX_DEBUG(this, nx::format("startDisplay %1").arg(m_resource->toResource()->getId()));

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
    return nullptr;
}

void QnClientVideoCamera::setLightCPUMode(QnAbstractVideoDecoder::DecodeMode val)
{
    m_camdispay.setLightCPUMode(val);
}

void QnClientVideoCamera::exportMediaPeriodToFile(const QnTimePeriod &timePeriod,
    const  QString& fileName,
    const QString& format,
    QnStorageResourcePtr storage,
    qint64 serverTimeZoneMs,
    const nx::core::transcoding::Settings& transcodingSettings,
    const QnTimePeriodList& playbackMask)
{
    using namespace nx::recording;

    qint64 startTimeUs = timePeriod.startTimeMs * 1000ll;
    NX_ASSERT(timePeriod.durationMs > 0, "Invalid time period, possibly LIVE is exported");
    qint64 endTimeUs = timePeriod.durationMs > 0
        ? timePeriod.endTimeMs() * 1000ll
        : DATETIME_NOW;

    NX_MUTEX_LOCKER lock( &m_exportMutex );
    if (!m_exportRecorder)
    {
        auto tmpReader = qnClientCoreModule->dataProviderFactory()->createDataProvider(
            m_resource->toResourcePtr());
        QnAbstractArchiveStreamReader* archiveReader = dynamic_cast<QnAbstractArchiveStreamReader*> (tmpReader);
        if (!archiveReader)
        {
            delete tmpReader;
            emit exportFinished(Error(Error::Code::invalidResourceType), fileName);
            return;
        }

        archiveReader->setCycleMode(false);
        archiveReader->setQuality(MEDIA_Quality_ForceHigh, true); // for 'mkv' and 'avi' files
        archiveReader->setPlaybackMask(playbackMask);

        // In the case of AVI, it is required to add filtering.
        if (fileName.toLower().endsWith(".avi"))
            archiveReader->addMediaFilter(std::make_shared<H2645Mp4ToAnnexB>());

        QnRtspClientArchiveDelegate* rtspClient = dynamic_cast<QnRtspClientArchiveDelegate*> (archiveReader->getArchiveDelegate());
        if (rtspClient) {
            // 'slow' open mode. send DESCRIBE and SETUP to server.
            // it is required for av_streams in output file - we should know all codec context immediately
            QnVirtualCameraResourcePtr camera = m_resource->toResourcePtr().dynamicCast<QnVirtualCameraResource>();
            rtspClient->setCamera(camera);
            rtspClient->setPlayNowModeAllowed(false);
            rtspClient->setMediaRole(PlaybackMode::export_);
        }

        m_exportReader = archiveReader;

        m_exportRecorder = new nx::vms::client::desktop::ExportStorageStreamRecorder(
            m_resource->toResourcePtr(), m_exportReader);

        connect(m_exportRecorder,   &QnStreamRecorder::recordingFinished, this,   &QnClientVideoCamera::stopExport);
        connect(m_exportRecorder,   &QnStreamRecorder::recordingProgress, this,   &QnClientVideoCamera::exportProgress);
        connect(m_exportRecorder,   &QnStreamRecorder::recordingFinished, this,   &QnClientVideoCamera::exportFinished);
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
    {
        m_exportRecorder->addRecordingContext(fileName, storage);
    }
    else if (!m_exportRecorder->addRecordingContext(fileName))
    {
        emit exportFinished(Error(Error::Code::fileCreate), fileName);
        return;
    }

    m_exportRecorder->setServerTimeZoneMs(serverTimeZoneMs);
    m_exportRecorder->setContainer(format);

    nx::core::transcoding::FilterChain filters(
        transcodingSettings, m_resource->getDewarpingParams(), m_resource->getVideoLayout());
    m_exportRecorder->setTranscodeFilters(filters);

    m_exportReader->addDataProcessor(m_exportRecorder);
    if (archiveReader)
        archiveReader->jumpTo(startTimeUs, startTimeUs);

    m_exportReader->start();
    m_exportRecorder->start();
}

void QnClientVideoCamera::stopExport()
{
    // This function is now synchronous - it will wait for other threads to finish.
    if (m_exportReader)
    {
        if (m_exportRecorder)
            m_exportReader->removeDataProcessor(m_exportRecorder);
        m_exportReader->pleaseStop();
        m_exportReader->wait(); //< We should wait for reader to stop.
        m_exportReader->deleteLater();
    }
    if (m_exportRecorder)
    {
        bool exportFinishedOk = m_exportRecorder->isFinished();
        if (!exportFinishedOk)
        {
            m_exportRecorder->pleaseStop();
            m_exportRecorder->wait(); //< We should wait for recorder to stop.
            // executeLater is used to make it work similar to previous code.
        }
        m_exportRecorder->deleteLater();
    }

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
    NX_MUTEX_LOCKER lock( &m_exportMutex );
    if (m_exportRecorder)
        return m_exportRecorder->fixedFileName();
    else
        return QString();
}
