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
            isValid(true),
            startTimeMs(startTimeMs),
            timeZone(timeZone),
            fileName(fileName),
            provider(provider)
        {
        }

        FileStartedInfo() = default;
    };

public:
    using FileWrittenHandler = std::function<void(
        std::chrono::milliseconds startTime,
        std::chrono::milliseconds duration)>;


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
        QnAbstractMediaStreamDataProvider* provider,
        bool sideRecorder = false) override;

    virtual void fileFinished(
        qint64 durationMs,
        const QString& fileName,
        QnAbstractMediaStreamDataProvider* provider,
        qint64 fileSize,
        qint64 startTimeMs = AV_NOPTS_VALUE) override;

private:
    FileStartedInfo m_lastfileStartedInfo;
    FileWrittenHandler m_fileWrittenHandler;
};
