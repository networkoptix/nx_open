#ifndef MEDIA_STREAM_STATISTICS_H
#define MEDIA_STREAM_STATISTICS_H

#include <QtCore/QDateTime>
#include <nx/utils/thread/mutex.h>

#define CL_STATISTICS_WINDOW_MS 2000
#define CL_STATISTICS_UPDATE_PERIOD_MS 700
#define CL_STATS_NUM  (CL_STATISTICS_WINDOW_MS/CL_STATISTICS_UPDATE_PERIOD_MS + 1)

// TODO: #Elric #enum
enum QnMediaStreamStatisticsEvent
{
    CL_STAT_DATA,
    CL_STAT_FRAME_LOST,
    CL_STAT_CAMRESETED,
    CL_STAT_END //< Must not be used in onEvent.
};

class QN_EXPORT QnMediaStreamStatistics
{
    struct EventStat
    {
        EventStat(): amount(0) {}

        QDateTime firstTime;
        QDateTime lastTime;
        unsigned long amount;
    };

    struct StatHelper
    {
        StatHelper(): total_data(0), frames(0) {}

        unsigned long total_data;
        unsigned long frames;
    };

public:
    QnMediaStreamStatistics();

    void resetStatistics(); // resets statistics; and make it runing
    void stop(); // stops the statistic;

    void onData(unsigned int datalen, bool isKeyFrame);// must be called then new data from cam arrived; if datalen==0 => timeout
    float getBitrateMbps() const; // returns instant bitrate at megabits
    float getFrameRate() const;// returns instant framerate
    int getFrameSize() const;// returns average frame size in kb( based on getBitrate and getFrameRate)
    float getAverageGopSize() const; // returns total frames count devided by key frames count
    float getavBitrate() const; // returns average bitrate
    float getavFrameRate() const;// returns average framerate
    unsigned long totalSecs() const; // how long statistics is assembled in seconds

    void onBadSensor();
    bool badSensor() const;

    void onLostConnection();
    bool isConnectionLost() const;
    int connectionLostSec() const;

    void onEvent(QnMediaStreamStatisticsEvent event);
    unsigned long totalEvents(QnMediaStreamStatisticsEvent event) const;
    float eventsPerHour(QnMediaStreamStatisticsEvent event) const;
    QDateTime firstOccurred(QnMediaStreamStatisticsEvent event) const;
    QDateTime lastOccurred(QnMediaStreamStatisticsEvent event) const;

    // ========

private:
    mutable QnMutex m_mutex;

    QDateTime m_startTime, m_stopTime;
    unsigned long m_frames;
    unsigned long m_keyFrames;
    unsigned long long m_dataTotal;

    bool m_badsensor;
    bool m_connectionLost;
    QDateTime m_connectionLostTime;

    EventStat m_events[CL_STAT_END];

    StatHelper m_stat[CL_STATS_NUM];
    int m_current_stat;
    bool m_first_ondata_call;
    QTime m_statTime;

    float m_bitrateMbps;
    float m_framerate;

    bool m_runing;
};

#endif // MEDIA_STREAM_STATISTICS_H
