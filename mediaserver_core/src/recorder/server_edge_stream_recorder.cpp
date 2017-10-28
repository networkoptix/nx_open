#include "server_edge_stream_recorder.h"

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
    QnAbstractMediaStreamDataProvider* provider)
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
    qint64 fileSize)
{
    if (m_lastfileStartedInfo.isValid)
    {
        base_type::fileStarted(
            m_lastfileStartedInfo.startTimeMs,
            m_lastfileStartedInfo.timeZone,
            m_lastfileStartedInfo.fileName,
            m_lastfileStartedInfo.provider);

        base_type::fileFinished(durationMs, fileName, provider, fileSize);

        if (m_fileWrittenHandler)
            m_fileWrittenHandler(m_lastfileStartedInfo.startTimeMs, durationMs);
    }

    m_lastfileStartedInfo = FileStartedInfo();
}
