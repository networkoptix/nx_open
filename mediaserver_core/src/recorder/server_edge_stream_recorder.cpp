#include "server_edge_stream_recorder.h"

#include <nx/utils/log/log.h>
#include <nx/utils/time.h>

namespace {

static const std::chrono::microseconds kDefaultPrebuffering(1);

} // namespace

using namespace std::chrono;

QnServerEdgeStreamRecorder::QnServerEdgeStreamRecorder(
    const QnResourcePtr &dev,
    QnServer::ChunksCatalog catalog,
    QnAbstractMediaStreamDataProvider* mediaProvider)
    :
    QnServerStreamRecorder(dev, catalog, mediaProvider)
{
    setPrebufferingUsec(duration_cast<microseconds>(kDefaultPrebuffering).count());
    setCanDropPackets(false);
}

QnServerEdgeStreamRecorder::~QnServerEdgeStreamRecorder()
{
    stop();
}

void QnServerEdgeStreamRecorder::setOnFileWrittenHandler(FileWrittenHandler handler)
{
    m_fileWrittenHandler = std::move(handler);
}

bool QnServerEdgeStreamRecorder::canAcceptData() const 
{
    return !isQueueFull();
}

void QnServerEdgeStreamRecorder::setSaveMotionHandler(MotionHandler handler)
{
    m_motionHandler = std::move(handler);
}

void QnServerEdgeStreamRecorder::setStartTimestampThreshold(
    const microseconds& threshold)
{
    m_threshold = threshold;
}

void QnServerEdgeStreamRecorder::setRecordingBounds(
    const std::chrono::microseconds& startTime,
    const std::chrono::microseconds& endTime)
{
    m_startRecordingBound = startTime;
    m_endRecordingBound = endTime;
}

void QnServerEdgeStreamRecorder::setEndOfRecordingHandler(
    nx::utils::MoveOnlyFunc<void()> endOfRecordingHandler)
{
    m_endOfRecordingHandler = std::move(endOfRecordingHandler);
}

bool QnServerEdgeStreamRecorder::saveMotion(const QnConstMetaDataV1Ptr& motion)
{
    if (m_motionHandler)
        return m_motionHandler(motion);

    return base_type::saveMotion(motion);
}

bool QnServerEdgeStreamRecorder::needSaveData(const QnConstAbstractMediaDataPtr& media)
{
    if (m_startRecordingBound == boost::none && m_endRecordingBound == boost::none)
        return true;

    const bool isEndRecordingBoundOk = m_endRecordingBound == boost::none
        || microseconds(media->timestamp) < *m_endRecordingBound;

    return m_needSaveData && isEndRecordingBoundOk;
}

void QnServerEdgeStreamRecorder::beforeProcessData(const QnConstAbstractMediaDataPtr& media)
{
    if (!media)
        return;

    if (media->dataType == QnAbstractMediaData::DataType::EMPTY_DATA)
    {
        m_endOfRecordingHandler();
        return;
    }

    if (m_startRecordingBound != boost::none)
    {
        if (microseconds(media->timestamp) >= m_startRecordingBound)
            m_needSaveData = true;
    }

    // Check if frame timestamp is too far in the past.
    bool needToCheckTimestamp = m_startRecordingBound != boost::none
        && m_endOfRecordingHandler
        && m_threshold != microseconds::zero();

    if (needToCheckTimestamp)
    {
        const auto frameTimestamp = microseconds(media->timestamp);
        const auto diff = *m_startRecordingBound - frameTimestamp;
        if (diff > m_threshold)
        {
            NX_WARNING(
                this,
                lm("Start timestamp is too far in the past from requested. "
                    "Requested timestmap: %1. Frame timestamp: %2")
                    .args(*m_startRecordingBound, frameTimestamp));

            m_endOfRecordingHandler();
            return;
        }
    }

    base_type::setLastMediaTime(media->timestamp);
    return;
}

bool QnServerEdgeStreamRecorder::saveData(const QnConstAbstractMediaDataPtr& mediaData)
{
    if (m_startRecordingBound == boost::none && m_endRecordingBound == boost::none)
        return base_type::saveData(mediaData);

    auto nonConstMediaData = std::const_pointer_cast<QnAbstractMediaData>(mediaData);
    if (!nonConstMediaData)
        return false;

    // We can use non const object if only 1 consumer.
    NX_ASSERT(nonConstMediaData->dataProvider->processorsCount() <= 1);
    const auto timestamp = microseconds(nonConstMediaData->timestamp);

    if (m_startRecordingBound != boost::none)
        nonConstMediaData->timestamp = std::max(timestamp, *m_startRecordingBound).count();

    if (m_endRecordingBound != boost::none && timestamp > *m_endRecordingBound)
    {
        if (m_endOfRecordingHandler)
            m_endOfRecordingHandler();

        nonConstMediaData->timestamp = m_endRecordingBound->count();
    }

    return base_type::saveData(mediaData);
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
            ? microseconds::zero()
            : m_startRecordingBound.get();

        const auto endRecordingBound = m_endRecordingBound == boost::none
            ? microseconds::zero()
            : m_endRecordingBound.get();

        NX_VERBOSE(this, lm(
            "File has been written. "
            "Start time: %1 (%2). "
            "End time: %3 (%4). "
            "Duration: %5. "
            "Recording bounds: %6 (%7) - %8 (%9). ")
                .args(
                    nx::utils::fromOffsetSinceEpoch(
                        milliseconds(m_lastfileStartedInfo.startTimeMs)),
                    milliseconds(m_lastfileStartedInfo.startTimeMs),
                    nx::utils::fromOffsetSinceEpoch(
                        milliseconds(m_lastfileStartedInfo.startTimeMs + durationMs)),
                    milliseconds(m_lastfileStartedInfo.startTimeMs + durationMs),
                    milliseconds(durationMs),
                    nx::utils::fromOffsetSinceEpoch(startRecordingBound),
                    duration_cast<milliseconds>(startRecordingBound).count(),
                    nx::utils::fromOffsetSinceEpoch(endRecordingBound),
                    duration_cast<milliseconds>(endRecordingBound).count()));

        if (m_fileWrittenHandler)
        {
            m_fileWrittenHandler(
                milliseconds(m_lastfileStartedInfo.startTimeMs),
                milliseconds(durationMs));
        }
    }

    m_lastfileStartedInfo = FileStartedInfo();
}
