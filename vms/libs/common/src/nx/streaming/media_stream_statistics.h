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
    float getFrameRate() const;// returns instant framerate
    float getAverageGopSize() const; // returns total frames count devided by key frames count
    bool hasMediaData() const;

    // Events related
    bool isConnectionLost() const;
    void onEvent(std::chrono::microseconds timestamp, CameraDiagnostics::Result event);
    void updateStatisticsUnsafe(CameraDiagnostics::Result event);
private:
    void QnMediaStreamStatistics::onData(
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
#if 0
    QDateTime m_startTime, m_stopTime;
    unsigned long m_frames = 0;
    unsigned long m_keyFrames = 0;
    unsigned long long m_dataTotal = 0;


    StatHelper m_stat[CL_STATS_NUM];
    int m_current_stat;
    bool m_first_ondata_call;
    QTime m_statTime;

    float m_bitrateMbps;
    float m_framerate;

    bool m_runing;
#endif
};
