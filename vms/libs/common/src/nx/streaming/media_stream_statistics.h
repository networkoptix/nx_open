#pragma once

#include <deque>
#include <chrono>

#include <nx/utils/thread/mutex.h>
#include <utils/camera/camera_diagnostics.h>

struct QnAbstractMediaData;
using QnAbstractMediaDataPtr = std::shared_ptr<QnAbstractMediaData>;

class QnMediaStreamStatistics: public QObject
{
    Q_OBJECT
signals:
    void streamEvent(CameraDiagnostics::Result event);
public:
    QnMediaStreamStatistics();

    void reset();
    void onData(const QnAbstractMediaDataPtr& media);
    qint64 bitrateBitsPerSecond() const;
    float getFrameRate() const;
    float getAverageGopSize() const;
    bool hasMediaData() const;

    // Events related
    bool isConnectionLost() const;
    void onEvent(std::chrono::microseconds timestamp, CameraDiagnostics::Result event);
private:
    void updateStatisticsUnsafe(CameraDiagnostics::Result event);
    void onData(
        std::chrono::microseconds timestamp, size_t dataSize,
        std::optional<bool> isKeyFrame = std::nullopt);

    mutable QnMutex m_mutex;

    struct Data
    {
        std::chrono::microseconds timestamp{};
        size_t size = 0;
        std::optional<bool> isKeyFrame = false;

        bool operator<(std::chrono::microseconds value) const { return timestamp < value; }
    };

    std::deque<Data> m_data;
    qint64 m_totalSizeBytes = 0;
    std::atomic_int m_numberOfErrors = 0;
};
