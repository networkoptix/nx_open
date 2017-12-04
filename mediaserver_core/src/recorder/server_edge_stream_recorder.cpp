#include "server_edge_stream_recorder.h"

#include <nx/utils/log/log.h>
#include <nx/utils/time.h>

QnServerEdgeStreamRecorder::QnServerEdgeStreamRecorder(
    const QnResourcePtr &dev,
    QnServer::ChunksCatalog catalog,
    QnAbstractMediaStreamDataProvider* mediaProvider)
    :
    QnServerStreamRecorder(dev, catalog, mediaProvider)
{
}

QnServerEdgeStreamRecorder::~QnServerEdgeStreamRecorder()
{
    stop();
}

void QnServerEdgeStreamRecorder::setOnFileWrittenHandler(FileWrittenHandler handler)
{
    m_fileWrittenHandler = std::move(handler);
}

void QnServerEdgeStreamRecorder::setSaveMotionHandler(MotionHandler handler)
{
    m_motionHandler = std::move(handler);
}

void QnServerEdgeStreamRecorder::setStartTimestampThreshold(
    const std::chrono::microseconds& threshold)
{
    m_threshold = threshold;
}

int64_t QnServerEdgeStreamRecorder::lastRecordedTimeUs() const
{
    return m_lastRecordedTime;
}

bool QnServerEdgeStreamRecorder::saveMotion(const QnConstMetaDataV1Ptr& motion)
{
    if (m_motionHandler)
        return m_motionHandler(motion);

    return base_type::saveMotion(motion);
}

bool QnServerEdgeStreamRecorder::needSaveData(const QnConstAbstractMediaDataPtr& media)
{
    return true;
}

bool QnServerEdgeStreamRecorder::beforeProcessData(const QnConstAbstractMediaDataPtr& media)
{
    if (!media)
        return false;

    if (m_terminated)
        return false;

    bool needToCheckTimestamp = m_startRecordingBound
        && m_endOfRecordingHandler
        && m_threshold != std::chrono::microseconds::zero();

    if (needToCheckTimestamp)
    {
        const auto frameTimestamp = std::chrono::microseconds(media->timestamp);
        const auto diff = *m_startRecordingBound - frameTimestamp;
        if (diff > m_threshold)
        {
            NX_WARNING(
                this,
                lm("Start timestamp is to far in the past from requested"
                    "Requested timestmap: %1. Frame timestamp: %2")
                    .args(*m_startRecordingBound, frameTimestamp));

            m_terminated = true;
            m_endOfRecordingHandler();
            return false;
        }
    }

    if (media->dataType != QnAbstractMediaData::DataType::EMPTY_DATA
        && media->timestamp != AV_NOPTS_VALUE)
    {
        m_lastRecordedTime = media->timestamp;
    }

    base_type::setLastMediaTime(media->timestamp);
    return true;
}

void QnServerEdgeStreamRecorder::fileStarted(
    qint64 startTimeMs,
    int timeZone,
    const QString& fileName,
    QnAbstractMediaStreamDataProvider* provider,
    bool /*sideRecorder*/)
{
    m_lastfileStartedInfo = FileStartedInfo(
        startTimeMs,
        timeZone,
        fileName,
        provider);
}

void QnServerEdgeStreamRecorder::fileFinished(
    qint64 durationMs,
    const QString& fileName,
    QnAbstractMediaStreamDataProvider* provider,
    qint64 fileSize,
    qint64 /*startTimeMs*/)
{
    using namespace std::chrono;

    if (m_lastfileStartedInfo.isValid)
    {
        base_type::fileStarted(
            m_lastfileStartedInfo.startTimeMs,
            m_lastfileStartedInfo.timeZone,
            m_lastfileStartedInfo.fileName,
            m_lastfileStartedInfo.provider,
            /*sideRecorder*/ true);

        base_type::fileFinished(
            durationMs,
            fileName,
            provider,
            fileSize,
            m_lastfileStartedInfo.startTimeMs);

        const auto startRecordingBound = m_startRecordingBound == boost::none
            ? std::chrono::microseconds::zero()
            : m_startRecordingBound.get();

        const auto endRecordingBound = m_endRecordingBound == boost::none
            ? std::chrono::microseconds::zero()
            : m_endRecordingBound.get();

        NX_VERBOSE(this, lm(
            "File has been written. "
            "Start time: %1 (%2ms). "
            "End time: %3 (%4ms). "
            "Duration: %5ms. "
            "Recording bounds: %6 (%7ms) - %8 (%9ms). ")
                .args(
                    nx::utils::fromOffsetSinceEpoch(
                        milliseconds(m_lastfileStartedInfo.startTimeMs)),
                    m_lastfileStartedInfo.startTimeMs,
                    nx::utils::fromOffsetSinceEpoch(
                        milliseconds(m_lastfileStartedInfo.startTimeMs + durationMs)),
                    m_lastfileStartedInfo.startTimeMs + durationMs,
                    durationMs,
                    nx::utils::fromOffsetSinceEpoch(startRecordingBound),
                    duration_cast<milliseconds>(startRecordingBound).count(),
                    nx::utils::fromOffsetSinceEpoch(endRecordingBound),
                    duration_cast<milliseconds>(endRecordingBound).count()));

        if (m_fileWrittenHandler)
        {
            m_fileWrittenHandler(
                std::chrono::milliseconds(
                    m_lastfileStartedInfo.startTimeMs),
                std::chrono::milliseconds(durationMs));
        }
    }

    m_lastfileStartedInfo = FileStartedInfo();
}
