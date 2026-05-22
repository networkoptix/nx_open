// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "stream_recorder.h"

#include <analytics/common/object_metadata.h>
#include <core/resource/avi/avi_archive_delegate.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_resource.h>
#include <core/resource/resource.h>
#include <export/sign_helper.h>
#include <nx/media/abstract_data_packet.h>
#include <nx/media/config.h>
#include <nx/media/media_data_packet.h>
#include <nx/streaming/abstract_media_stream_data_provider.h>
#include <nx/streaming/archive_stream_reader.h>
#include <nx/utils/log/log.h>
#include <nx/vms/common/system_settings.h>
#include <utils/common/util.h>

namespace { static const int STORE_QUEUE_SIZE = 50; } // namespace

QnStreamRecorder::QnStreamRecorder(const QnResourcePtr& dev)
:
    QnAbstractDataConsumer(STORE_QUEUE_SIZE),
    m_resource(dev),
    m_mediaDevice(dev.dynamicCast<QnMediaResource>())
{
    memset(m_gotKeyFrame, 0, sizeof(m_gotKeyFrame)); // false
}

QnStreamRecorder::~QnStreamRecorder()
{
    stop();
}

void QnStreamRecorder::close()
{
    if (!m_finishReported)
    {
        m_finishReported = true;
        reportFinished();
    }

    if (!m_fileOpened)
    {
        NX_VERBOSE(this, "%1: File has not been opened", __func__);
        return; //< If a file has not been opened there's no need to close it.
    }

    const auto durationMs = std::chrono::milliseconds(std::max<int64_t>(
        m_endDateTimeUs != qint64(AV_NOPTS_VALUE)
            ? m_endDateTimeUs / 1000 - startTimeUs() / 1000 : 0,
        0));

    closeRecordingContext(std::chrono::milliseconds(startTimeUs() / 1000), durationMs); //< Different actions per context (cloud, storage, etc)
    markNeedKeyData();
    m_firstTime = true;
    m_fileOpened = false;

    afterClose(); //< Different actions per recorder (server, export. etc)
    m_endDateTimeUs = m_startDateTimeUs = AV_NOPTS_VALUE;
}

void QnStreamRecorder::markNeedKeyData()
{
    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
        m_gotKeyFrame[i] = false;
}

void QnStreamRecorder::updateProgress(qint64 timestampUs)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (m_bofDateTimeUs != qint64(AV_NOPTS_VALUE) && m_eofDateTimeUs != qint64(AV_NOPTS_VALUE)
        && m_eofDateTimeUs > m_bofDateTimeUs)
    {
        int progress =
            ((timestampUs - m_bofDateTimeUs) * 100LL) / (m_eofDateTimeUs - m_bofDateTimeUs);

        // That happens quite often.
        if (progress > 100)
            progress = 100;

        if (progress != m_lastProgress && progress >= 0)
        {
            NX_VERBOSE(this, "Recording progress %1", progress);
            m_lastProgress = progress;
            lock.unlock();
            emit recordingProgress(progress);
        }
    }
}

int QnStreamRecorder::getStreamIndex(const QnConstAbstractMediaDataPtr& mediaData) const
{
    return mediaData->channelNumber;
}

bool QnStreamRecorder::processData(const QnAbstractDataPacketPtr& data)
{
    const auto md = std::dynamic_pointer_cast<const QnAbstractMediaData>(data);
    if (!md)
        return true; //< skip unknown data

    NX_VERBOSE(this, "Process data %1", md);
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (m_eofDateTimeUs != qint64(AV_NOPTS_VALUE) && md->timestamp > m_eofDateTimeUs)
        {
            if (m_firstTime)
                setLastError(nx::recording::Error::Code::dataNotFound);

            pleaseStop();
            return true;
        }
    }

    saveData(md);
    updateProgress(md->timestamp);
    return true;
}

bool QnStreamRecorder::isAudioRecorded() const
{
    if (auto camera = m_resource.dynamicCast<QnVirtualCameraResource>())
        return camera->isAudioRecorded();
    return true;
}

bool QnStreamRecorder::prepareToStart(const QnConstAbstractMediaDataPtr& mediaData)
{
    m_endDateTimeUs = mediaData->timestamp;
    if (m_startDateTimeUs == (int64_t)AV_NOPTS_VALUE)
        m_startDateTimeUs = mediaData->timestamp;
    m_isAudioPresent = m_audioLayout && !m_audioLayout->tracks().empty() && isAudioRecorded();
    const auto audioLayout = m_isAudioPresent ? m_audioLayout : AudioLayoutConstPtr();
    auto metaData = prepareMetaData(mediaData, m_videoLayout);
    if (!doPrepareToStart(mediaData, m_videoLayout, audioLayout, metaData))
        return false;

    onSuccessfulPrepare();
    m_fileOpened = true;
    m_finishReported = false;
    return true;
}

bool QnStreamRecorder::dataHoleDetected(const QnConstAbstractMediaDataPtr& md)
{
    if (m_endDateTimeUs != (int64_t) AV_NOPTS_VALUE
        && (md->dataType == QnAbstractMediaData::VIDEO || md->dataType == QnAbstractMediaData::AUDIO)
        && md->timestamp < m_endDateTimeUs - 1000LL * 1000)
    {
        NX_DEBUG(
            this, "Time translated into the past for %1 s(%2 - %3).  Closing file",
            (md->timestamp - m_endDateTimeUs) / 1'000'000, md->timestamp, m_endDateTimeUs);

        return true;
    }

    return false;
}

void QnStreamRecorder::setUtcOffsetAllowed(bool value)
{
    m_isUtcOffsetAllowed = value;
}

QnAviArchiveMetadata QnStreamRecorder::prepareMetaData(
    const QnConstAbstractMediaDataPtr&, const QnConstResourceVideoLayoutPtr& videoLayout)
{
    QnAviArchiveMetadata result;
    result.version = QnAviArchiveMetadata::kVersionBeforeTheIntegrityCheck;
    if (m_isUtcOffsetAllowed)
        result.startTimeMs = m_startDateTimeUs / 1000LL;

    result.dewarpingParams = m_mediaDevice->getDewarpingParams();
    result.encryptionData = prepareEncryptor((quint64) result.startTimeMs);
    result.videoLayoutSize = videoLayout ? videoLayout->size() : QSize(1, 1);
    result.videoLayoutChannels = videoLayout ? videoLayout->getChannels() : QList<int>() << 0;
    if (m_mediaDevice->customAspectRatio().isValid())
        result.overridenAr = m_mediaDevice->customAspectRatio().toFloat();
    adjustMetaData(result);
    return result;
}

void QnStreamRecorder::setPreciseStartDateTime(int64_t startTimeUs)
{
    m_startDateTimeUs = startTimeUs;
}

int64_t QnStreamRecorder::packetCount() const
{
    return m_packetCount;
}

bool QnStreamRecorder::saveData(const QnConstAbstractMediaDataPtr& md)
{
    using namespace nx::common::metadata;
    if (auto motionPacket = std::dynamic_pointer_cast<const QnMetaDataV1>(md))
    {
        NX_VERBOSE(this, "Got motion packet, timestamp %1 us", md->timestamp);
        saveMotion(motionPacket);
    }
    else if (auto metadata = std::dynamic_pointer_cast<const QnCompressedMetadata>(md);
        metadata && metadata->metadataType == MetadataType::ObjectDetection)
    {
        NX_VERBOSE(this, "Got analytics packet, timestamp %1 us", md->timestamp);
        const auto analytics =
            std::dynamic_pointer_cast<const QnCompressedObjectMetadataPacket>(metadata);
    }

    if (m_startDateTimeUs != (int64_t) AV_NOPTS_VALUE
        && md->timestamp < m_startDateTimeUs
        && (md->dataType == QnAbstractMediaData::GENERIC_METADATA
        || md->dataType == QnAbstractMediaData::AUDIO))
    {
        NX_VERBOSE(this,
            "Timestamp of metadata (%1us) is less than the start timestamp of the file "
            "being recorded currently (%2us), ignoring metadata",
            md->timestamp, m_startDateTimeUs);

        return true;
    }

    if (md->dataType != QnAbstractMediaData::GENERIC_METADATA && dataHoleDetected(md))
    {
        NX_VERBOSE(this, "Closing file %1 because data hole has been detected", startTimeUs());
        close();
    }

    QnConstCompressedVideoDataPtr vd = std::dynamic_pointer_cast<const QnCompressedVideoData>(md);
    if (vd && !m_gotKeyFrame[vd->channelNumber] && !(vd->flags & AV_PKT_FLAG_KEY))
    {
        NX_VERBOSE(
            this, "saveData(): VIDEO; skip data. Timestamp: %1 (%2ms)",
            QDateTime::fromMSecsSinceEpoch(md->timestamp / 1000), vd->timestamp / 1000);

        return true; // skip data
    }

    if (m_firstTime)
    {
        bool canStartNewFile = m_hasVideo
            ? md->dataType == QnAbstractMediaData::VIDEO && (md->flags & AV_PKT_FLAG_KEY)
            : md->dataType == QnAbstractMediaData::AUDIO;
        if (!canStartNewFile)
        {
            NX_DEBUG(this, "Skip packet before first video(or audio in case audio only) packet: %1"
                "(%2ms), data type: %3, has video: %4",
                QDateTime::fromMSecsSinceEpoch(md->timestamp / 1000),
                md->timestamp / 1000,
                md->dataType,
                m_hasVideo);
            return true; // skip audio/meta packets before first video packet
        }

        if (!prepareToStart(md))
        {
            NX_WARNING(this, "saveData: prepareToStart() failed");
            m_needStop = true;
            return false;
        }

        NX_VERBOSE(this, "saveData(): first time; recording started");
        m_firstTime = false;
        emit recordingStarted();
    }

    int channel = md->channelNumber;
    if (md->flags & AV_PKT_FLAG_KEY)
        m_gotKeyFrame[channel] = true;

    int streamIndex = getStreamIndex(md);
    if (streamIndex > streamCount())
    {
        NX_DEBUG(
            this, "saveData: skipping packet because streamIndex (%1) > stream count (%2)",
            streamIndex, streamCount());

        return true;
    }

    if (md->dataType == QnAbstractMediaData::VIDEO || md->dataType == QnAbstractMediaData::AUDIO)
        m_endDateTimeUs = std::max(m_endDateTimeUs, (int64_t) md->timestamp);

    const auto frameMediaType = toAvMediaType(md->dataType);
    const auto contextMediaType = streamAvMediaType(md->dataType, streamIndex);
    if (contextMediaType == AVMEDIA_TYPE_UNKNOWN || frameMediaType != contextMediaType)
    {
        NX_DEBUG(
            this, "Skipping a frame of the unexpected type. AVMediaType Expected: %1. "
            "Actual: %2", contextMediaType, frameMediaType);

        return true;
    }

    auto data = encryptDataIfNeed(md);
    if (!data)
    {
        NX_DEBUG(this, "Stop recording due to encryption error");
        m_needStop = true;
        return true;
    }

    writeData(data, streamIndex);
    if (getLastError())
    {
        NX_DEBUG(this, "Stop recording due to error: %1", getLastError());
        m_needStop = true;
    }
    else
    {
        ++m_packetCount;
        onSuccessfulWriteData(md);
    }

    return true;
}

void QnStreamRecorder::reportFinished()
{
    emit recordingFinished(getLastError(), QString());
}

void QnStreamRecorder::endOfRun()
{
    NX_VERBOSE(this, "Closing file %1 from endOfRun", startTimeUs());
    close();
}

void QnStreamRecorder::setProgressBounds(qint64 bof, qint64 eof)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_bofDateTimeUs = bof;
    m_eofDateTimeUs = eof;
    m_lastProgress = -1;
}

bool QnStreamRecorder::isAudioPresent() const
{
    return m_isAudioPresent;
}

int64_t QnStreamRecorder::startTimeUs() const
{
    return m_startDateTimeUs;
}

QnConstAbstractMediaDataPtr QnStreamRecorder::encryptDataIfNeed(const QnConstAbstractMediaDataPtr& data)
{
    return data;
}

AudioLayoutConstPtr QnStreamRecorder::getAudioLayout()
{
    return m_audioLayout;
}

QnConstResourceVideoLayoutPtr QnStreamRecorder::getVideoLayout()
{
    return m_videoLayout;
}

void QnStreamRecorder::setVideoLayout(const QnConstResourceVideoLayoutPtr& videoLayout)
{
    m_videoLayout = videoLayout;
}

void QnStreamRecorder::setAudioLayout(const AudioLayoutConstPtr& audioLayout)
{
    m_audioLayout = audioLayout;
}

void QnStreamRecorder::setHasVideo(bool hasVideo)
{
    m_hasVideo = hasVideo;
}
