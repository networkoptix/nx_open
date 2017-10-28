#pragma once

#include <recorder/server_stream_recorder.h>

class QnServerEdgeStreamRecorder: public QnServerStreamRecorder
{
    using base_type = QnServerStreamRecorder;

    struct FileStartedInfo
    {
        bool isValid = false;
        qint64 startTimeMs = 0;
        int timeZone = 0;
        QString fileName;
        QnAbstractMediaStreamDataProvider* provider = nullptr;

        FileStartedInfo(
            qint64 startTimeMs,
            int timeZone,
            QString fileName,
            QnAbstractMediaStreamDataProvider* provider)
            :
            startTimeMs(startTimeMs),
            timeZone(timeZone),
            fileName(fileName),
            provider(provider),
            isValid(true)
        {
        }

        FileStartedInfo() = default;
    };

public:
    using FileWrittenHandler = std::function<void(
        int64_t startTimeMs, int64_t durationMs)>;


public:
    QnServerEdgeStreamRecorder(
        const QnResourcePtr &dev,
        QnServer::ChunksCatalog catalog,
        QnAbstractMediaStreamDataProvider* mediaProvider);

    virtual ~QnServerEdgeStreamRecorder();

    void setOnFileWrittenHandler(FileWrittenHandler handler);

protected:
    virtual bool needSaveData(const QnConstAbstractMediaDataPtr& media) override;
    virtual void beforeProcessData(const QnConstAbstractMediaDataPtr& media) override;

    virtual void fileStarted(
        qint64 startTimeMs,
        int timeZone,
        const QString& fileName,
        QnAbstractMediaStreamDataProvider* provider) override;

    virtual void fileFinished(
        qint64 durationMs,
        const QString& fileName,
        QnAbstractMediaStreamDataProvider* provider,
        qint64 fileSize) override;

private:
    FileStartedInfo m_lastfileStartedInfo;
    FileWrittenHandler m_fileWrittenHandler;
};