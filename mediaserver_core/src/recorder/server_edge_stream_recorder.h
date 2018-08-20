#pragma once

#include <recorder/server_stream_recorder.h>
#include <nx/utils/move_only_func.h>

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
    using FileWrittenHandler = nx::utils::MoveOnlyFunc<void(
        std::chrono::milliseconds startTime,
        std::chrono::milliseconds duration)>;

    using MotionHandler =
        nx::utils::MoveOnlyFunc<bool(const QnConstMetaDataV1Ptr& motion)>;

public:
    QnServerEdgeStreamRecorder(
        const QnResourcePtr &dev,
        QnServer::ChunksCatalog catalog,
        QnAbstractMediaStreamDataProvider* mediaProvider);

    virtual ~QnServerEdgeStreamRecorder();

    /**
     * Called each time archive file has been written.
     */
    void setOnFileWrittenHandler(FileWrittenHandler handler);

    /**
     * Motion handler overrides base class behavior.
     */
    void setSaveMotionHandler(MotionHandler handler);

    /**
     * If first frame has timestamp that is earlier than start recording border
     * and the difference is more than threshold then recording will not start.
     */
    void setStartTimestampThreshold(const std::chrono::microseconds& threshold);

    void setRecordingBounds(
        const std::chrono::microseconds& startTime,
        const std::chrono::microseconds& endTime);

    void setEndOfRecordingHandler(nx::utils::MoveOnlyFunc<void()> endOfRecordingHandler);

protected:
    virtual bool saveMotion(const QnConstMetaDataV1Ptr& motion) override;
    virtual bool canAcceptData() const override;
    virtual bool needSaveData(const QnConstAbstractMediaDataPtr& media) override;
    virtual void beforeProcessData(const QnConstAbstractMediaDataPtr& media) override;
    virtual bool saveData(const QnConstAbstractMediaDataPtr& md) override;

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
    MotionHandler m_motionHandler;
    std::chrono::microseconds m_threshold{0};
    bool m_terminated = false;
    bool m_needSaveData = false;

    boost::optional<std::chrono::microseconds> m_startRecordingBound;
    boost::optional<std::chrono::microseconds> m_endRecordingBound;
    nx::utils::MoveOnlyFunc<void()> m_endOfRecordingHandler;

};
