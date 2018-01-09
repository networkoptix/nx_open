#include "server_edge_stream_recorder.h"

#include <nx/utils/log/log.h>

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
    m_fileWrittenHandler = handler;
}

bool QnServerEdgeStreamRecorder::canAcceptData() const 
{
    return !isQueueFull();
}

bool QnServerEdgeStreamRecorder::needSaveData(const QnConstAbstractMediaDataPtr& media)
{
    return true;
}

void QnServerEdgeStreamRecorder::beforeProcessData(const QnConstAbstractMediaDataPtr& media)
{
    setLastMediaTime(media->timestamp);
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

        NX_VERBOSE(this, lm(
            "File has been written. "
            "Start time: %1 (%2ms). "
            "End time: %3 (%4ms). "
            "Duration: %5ms. "
            "Recording bounds: %6 (%7ms) - %8 (%9ms). ")
                .args(
                    QDateTime::fromMSecsSinceEpoch(m_lastfileStartedInfo.startTimeMs),
                    m_lastfileStartedInfo.startTimeMs,
                    QDateTime::fromMSecsSinceEpoch(m_lastfileStartedInfo.startTimeMs + durationMs),
                    m_lastfileStartedInfo.startTimeMs + durationMs,
                    durationMs,
                    QDateTime::fromMSecsSinceEpoch(m_startRecordingBound->count() / 1000),
                    m_startRecordingBound->count() / 1000,
                    QDateTime::fromMSecsSinceEpoch(m_endRecordingBound->count() / 1000),
                    m_endRecordingBound->count() / 1000));

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
