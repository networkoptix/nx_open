#pragma once

#include <QtCore/QDateTime>

#include <nx/utils/thread/mutex.h>
#include <utils/camera/camera_diagnostics.h>

class QnMediaStreamStatistics: public QObject
{
    Q_OBJECT

    static const int CL_STATISTICS_WINDOW_MS = 2000;
    static const int CL_STATISTICS_UPDATE_PERIOD_MS = 700;
    static const int CL_STATS_NUM = (CL_STATISTICS_WINDOW_MS / CL_STATISTICS_UPDATE_PERIOD_MS + 1);

    struct StatHelper
    {
        StatHelper() = default;

        unsigned long total_data = 0;
        unsigned long frames = 0;
    };
signals:
    void streamEvent(CameraDiagnostics::Result event);
public:
    QnMediaStreamStatistics();

    void resetStatistics(); // resets statistics; and make it runing
    void stop(); // stops the statistic;

    qint64 getTotalData() const;
    void onData(unsigned int datalen, bool isKeyFrame);// must be called then new data from cam arrived; if datalen==0 => timeout
    float getBitrateMbps() const; // returns instant bitrate at megabits
    float getFrameRate() const;// returns instant framerate
    int getFrameSize() const;// returns average frame size in kb( based on getBitrate and getFrameRate)
    float getAverageGopSize() const; // returns total frames count devided by key frames count
    float getavBitrate() const; // returns average bitrate
    float getavFrameRate() const;// returns average framerate
    unsigned long totalSecs() const; // how long statistics is assembled in seconds

    bool isConnectionLost() const;

    void onEvent(CameraDiagnostics::Result event);
    void updateStatisticsUnsafe(CameraDiagnostics::Result event);
private:
    mutable QnMutex m_mutex;

    QDateTime m_startTime, m_stopTime;
    unsigned long m_frames;
    unsigned long m_keyFrames;
    unsigned long long m_dataTotal;

    std::atomic_int m_numberOfErrors = 0;

    StatHelper m_stat[CL_STATS_NUM];
    int m_current_stat;
    bool m_first_ondata_call;
    QTime m_statTime;

    float m_bitrateMbps;
    float m_framerate;

    bool m_runing;
};
