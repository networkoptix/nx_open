// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "stream_recorder.h"

#include "export/sign_helper.h"
#include <common/common_module.h>
#include <core/resource/avi/avi_archive_delegate.h>
#include <core/resource/media_resource.h>
#include <core/resource/resource_consumer.h>
#include <core/resource/resource.h>
#include <core/resource/security_cam_resource.h>
#include <nx/fusion/model_functions.h>
#include <nx/streaming/abstract_data_packet.h>
#include <nx/streaming/abstract_media_stream_data_provider.h>
#include <nx/streaming/archive_stream_reader.h>
#include <nx/streaming/config.h>
#include <nx/streaming/media_data_packet.h>
#include <nx/utils/log/log.h>
#include <nx/vms/common/system_settings.h>
#include <utils/common/util.h>

namespace {

static const int STORE_QUEUE_SIZE = 50;
static const int kPrebufferHardLimit = 1000;

} // namespace

QnStreamRecorder::QnStreamRecorder(const QnResourcePtr& dev)
:
    QnAbstractDataConsumer(STORE_QUEUE_SIZE),
    QnResourceConsumer(dev),
    m_mediaDevice(m_resource.dynamicCast<QnMediaResource>())
{
    memset(m_gotKeyFrame, 0, sizeof(m_gotKeyFrame)); // false
    memset(m_motionFileList, 0, sizeof(m_motionFileList));
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

    closeRecordingContext(durationMs); //< Different actions per context (cloud, storage, etc)
    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
    {
        if (m_motionFileList[i])
            m_motionFileList[i]->close();
    }

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

void QnStreamRecorder::flushPrebuffer()
{
    if (!m_prebuffer.isEmpty())
        NX_VERBOSE(this, "Flushing prebuffer for resource %1", m_resource);

    while (!m_prebuffer.isEmpty())
    {
        QnConstAbstractMediaDataPtr d;
        m_prebuffer.pop(d);
        if (needSaveData(d))
            saveData(d);
        else
            markNeedKeyData();
    }
    m_nextIFrameTime = m_lastPrimaryTime = AV_NOPTS_VALUE;
}

qint64 QnStreamRecorder::isPrimaryStream(const QnConstAbstractMediaDataPtr& md) const
{
    if (!m_hasVideo)
        return md->dataType == QnAbstractMediaData::AUDIO;
    return md->dataType == QnAbstractMediaData::VIDEO && md->channelNumber == 0;
}

qint64 QnStreamRecorder::isPrimaryKeyFrame(const QnConstAbstractMediaDataPtr& md) const
{
    if (!m_hasVideo)
        return md->dataType == QnAbstractMediaData::AUDIO;
    return md->dataType == QnAbstractMediaData::VIDEO
        && md->channelNumber == 0
        && md->flags.testFlag(QnAbstractMediaData::MediaFlags_AVKey);
}

qint64 QnStreamRecorder::findNextIFrame(qint64 baseTime)
{
    for (int i = 0; i < m_prebuffer.size(); ++i)
    {
        const QnConstAbstractMediaDataPtr& media = m_prebuffer.at(i);
        if (isPrimaryKeyFrame(media) && media->timestamp > baseTime)
            return media->timestamp;
    }
    return AV_NOPTS_VALUE;
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
    if (m_needReopen)
    {
        m_needReopen = false;
        close();
    }

    const QnConstAbstractMediaDataPtr md =
        std::dynamic_pointer_cast<const QnAbstractMediaData>(data);

    if (!md)
        return true; //< skip unknown data

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

    m_prebuffer.push(md);
    const bool alwaysSave = md->flags & QnAbstractMediaData::MediaFlags_AlwaysSave;
    const bool timeDiscontinue = isPrimaryStream(md) && md->timestamp < m_lastPrimaryTime;
    const bool hardLimitReached = m_prebuffer.size() > kPrebufferHardLimit;
    const qint64 kNoPtsValue = (qint64) AV_NOPTS_VALUE;
    if (m_prebufferingUsec == 0 || alwaysSave || timeDiscontinue || hardLimitReached)
    {
        if (timeDiscontinue)
            NX_VERBOSE(this, "Time discontinue. Resource: %1", m_resource);
        if (hardLimitReached)
            NX_VERBOSE(this, "Pre-buffer hard limit reached. Resource: %1", m_resource);
        flushPrebuffer();
    }
    else if (isPrimaryStream(md))
    {
        m_lastPrimaryTime = md->timestamp;
        if (m_nextIFrameTime == kNoPtsValue && isPrimaryKeyFrame(md))
            m_nextIFrameTime = md->timestamp;
        while (m_nextIFrameTime != kNoPtsValue && md->timestamp - m_nextIFrameTime >= m_prebufferingUsec)
        {
            while (!m_prebuffer.isEmpty() && (!isPrimaryStream(m_prebuffer.front())
                || m_prebuffer.front()->timestamp < m_nextIFrameTime))
            {
                QnConstAbstractMediaDataPtr d;
                m_prebuffer.pop(d);
                if (needSaveData(d))
                    saveData(d);
                else if (md->dataType == QnAbstractMediaData::VIDEO)
                    markNeedKeyData();
            }
            m_nextIFrameTime = findNextIFrame(m_nextIFrameTime);
        }
    }

    updateProgress(md->timestamp);
    return true;
}

bool QnStreamRecorder::isAudioRecorded() const
{
    if (auto camera = m_resource.dynamicCast<QnSecurityCamResource>())
        return camera->isAudioRecorded();
    return true;
}

bool QnStreamRecorder::prepareToStart(const QnConstAbstractMediaDataPtr& mediaData)
{
    m_endDateTimeUs = m_startDateTimeUs = mediaData->timestamp;
    m_interleavedStream = m_videoLayout && m_videoLayout->channelCount() > 1;
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
    if (m_startDateTimeUs != (int64_t) AV_NOPTS_VALUE && md->timestamp < m_startDateTimeUs - 1000LL * 1000)
    {
        NX_DEBUG(
            this, "Time translated into the past for %1 s. Closing file",
            (md->timestamp - m_startDateTimeUs) / 1'000'000);

        return true;
    }

    return false;
}

QnAviArchiveMetadata QnStreamRecorder::prepareMetaData(
    const QnConstAbstractMediaDataPtr& mediaData, const QnConstResourceVideoLayoutPtr& videoLayout)
{
    QnAviArchiveMetadata result;
    result.version = QnAviArchiveMetadata::kVersionBeforeTheIntegrityCheck;
    if (isUtcOffsetAllowed())
        result.startTimeMs = mediaData->timestamp / 1000LL;

    result.dewarpingParams = m_mediaDevice->getDewarpingParams();
    result.encryptionData = prepareEncryptor((quint64) result.startTimeMs);
    result.videoLayoutSize = videoLayout ? videoLayout->size() : QSize(1, 1);
    result.videoLayoutChannels = videoLayout ? videoLayout->getChannels() : QVector<int>() << 0;
    if (m_mediaDevice->customAspectRatio().isValid())
        result.overridenAr = m_mediaDevice->customAspectRatio().toFloat();
    adjustMetaData(result);
    return result;
}

void QnStreamRecorder::setPreciseStartDateTime(int64_t startTimeUs)
{
    m_startDateTimeUs = startTimeUs;
}

bool QnStreamRecorder::saveData(const QnConstAbstractMediaDataPtr& md)
{
    if (auto motionPacket = std::dynamic_pointer_cast<const QnMetaDataV1>(md))
    {
        NX_VERBOSE(this, "QnStreamRecorder::saveData(): Motion, timestamp %1 us", md->timestamp);
        saveMotion(motionPacket);
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

    if (dataHoleDetected(md))
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

    if ((((md->flags & AV_PKT_FLAG_KEY) && md->dataType == QnAbstractMediaData::VIDEO)
        || !m_hasVideo) && needToTruncate(md))
    {
        m_endDateTimeUs = md->timestamp;
        NX_VERBOSE(this, "Truncating file %1", startTimeUs());
        close();
        m_endDateTimeUs = m_startDateTimeUs = md->timestamp;
    }

    if (m_firstTime)
    {
        if (vd == 0 && m_hasVideo)
        {
            NX_VERBOSE(
                this, "saveData(): AUDIO; skip audio packets before first video packet. Timestamp: %1 (%2ms)",
                QDateTime::fromMSecsSinceEpoch(md->timestamp / 1000), md->timestamp / 1000);

            return true; // skip audio packets before first video packet
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

    // Slow analytics plugin can provide metadata packets with timestamps less than latest video packet.
    // Reordering buffer tries to fix it, but the metadata packet can have too old timestamp if delay is larger
    // than the maximum size of the reordering buffer.
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

void QnStreamRecorder::setMotionFileList(QSharedPointer<QBuffer> motionFileList[CL_MAX_CHANNELS])
{
    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
        m_motionFileList[i] = motionFileList[i];
}

void QnStreamRecorder::setPrebufferingUsec(int value)
{
    m_prebufferingUsec = value;
}

int QnStreamRecorder::getPrebufferingUsec() const
{
    return m_prebufferingUsec;
}

bool QnStreamRecorder::needSaveData(const QnConstAbstractMediaDataPtr& /*media*/)
{
    return true;
}

bool QnStreamRecorder::saveMotion(const QnConstMetaDataV1Ptr& motion)
{
    if (motion && !motion->isEmpty() && m_motionFileList[motion->channelNumber])
    {
        NX_VERBOSE(this,
            "%1: Saving motion, timestamp %2 us, resource: %3",
            __func__, motion->timestamp, m_resource);
        motion->serialize(m_motionFileList[motion->channelNumber].data());
    }

    return true;
}

void QnStreamRecorder::setProgressBounds(qint64 bof, qint64 eof)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_bofDateTimeUs = bof;
    m_eofDateTimeUs = eof;
    m_lastProgress = -1;
}

void QnStreamRecorder::setNeedReopen()
{
    m_needReopen = true;
}

bool QnStreamRecorder::isAudioPresent() const
{
    return m_isAudioPresent;
}

void QnStreamRecorder::disconnectFromResource()
{
    stop();
    QnResourceConsumer::disconnectFromResource();
}

int64_t QnStreamRecorder::startTimeUs() const
{
    return m_startDateTimeUs;
}

bool QnStreamRecorder::isInterleavedStream() const
{
    return m_interleavedStream;
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
