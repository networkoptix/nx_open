#pragma once

#include <deque>
#include <chrono>

#include <nx/utils/thread/mutex.h>
#include <utils/camera/camera_diagnostics.h>

struct QnAbstractMediaData;
using QnAbstractMediaDataPtr = std::shared_ptr<QnAbstractMediaData>;

class QnMediaStreamStatistics: public QObject
{
public:
    QnMediaStreamStatistics();

    void reset();
    void onData(const QnAbstractMediaDataPtr& media);
    qint64 bitrateBitsPerSecond() const;
    float getFrameRate() const;
    float getAverageGopSize() const;
    bool hasMediaData() const;

private:
    struct Data
    {
        std::chrono::microseconds timestamp{};
        size_t size = 0;
        bool isKeyFrame = false;

        bool operator<(std::chrono::microseconds value) const { return timestamp < value; }
    };

    std::chrono::microseconds intervalUnsafe() const;
    void onData(std::chrono::microseconds timestamp, size_t dataSize, bool isKeyFrame);

    mutable QnMutex m_mutex;
    std::deque<Data> m_data;
    qint64 m_totalSizeBytes = 0;
};
