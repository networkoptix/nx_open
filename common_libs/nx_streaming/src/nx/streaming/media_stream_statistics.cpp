#include "media_stream_statistics.h"

QnMediaStreamStatistics::QnMediaStreamStatistics()
{
    resetStatistics();
}

void QnMediaStreamStatistics::resetStatistics()
{
    QnMutexLocker locker( &m_mutex );
    m_startTime = QDateTime::currentDateTime();
    m_frames = 0;
    m_dataTotal = 0;
    m_badsensor = false;

    for (int i = 0; i < CL_STAT_END; ++i)
        m_events[i].amount = 0;

    for (int i = 0; i < CL_STATS_NUM; ++i)
    {
        m_stat[i].frames = 0;
        m_stat[i].total_data = 0;
    }

    m_current_stat = 0;

    m_bitrateMbps = 0;
    m_framerate = 0;

    m_runing = true;

    m_first_ondata_call = true;

    m_connectionLost = false;

}

void QnMediaStreamStatistics::stop()
{
    if (!m_runing) return; 
    QnMutexLocker locker( &m_mutex );
    m_stopTime = QDateTime::currentDateTime();
    m_runing = false;
}

void QnMediaStreamStatistics::onData(unsigned int datalen, bool isKeyFrame)
{
    if (!m_runing) return; 
    QnMutexLocker locker( &m_mutex );

    if (datalen)
    {
        ++m_frames;
        if (isKeyFrame)
            ++m_keyFrames;

        m_dataTotal+=datalen;
        m_connectionLost = false;
    }

    if (m_first_ondata_call)
    {
        m_statTime.start();
        m_first_ondata_call = false;
    }

    int ms = m_statTime.elapsed();
    int ststat_num = ms/CL_STATISTICS_UPDATE_PERIOD_MS;

    if (ststat_num) // if we need to use next cell
    {
        m_statTime = m_statTime.addMSecs(ststat_num*CL_STATISTICS_UPDATE_PERIOD_MS);

        for (int i = 0; i < ststat_num;++i)
        {
            int index  = (m_current_stat + i + 1)%CL_STATS_NUM;
            m_stat[index].frames = 0;
            m_stat[index].total_data = 0;
        }

        m_current_stat = (m_current_stat + ststat_num)%CL_STATS_NUM;

        // need to recalculate new bitrate&framerate

        int total_frames = 0;
        int total_bytes = 0;
        for(int i = 0; i < CL_STATS_NUM; ++i)
        {
            if (i!=m_current_stat)
            {
                total_bytes += m_stat[i].total_data;
                total_frames += m_stat[i].frames;
            }
        }

        int time = (CL_STATS_NUM-1)*CL_STATISTICS_UPDATE_PERIOD_MS;

        m_bitrateMbps = total_bytes*8*1000.0/time/(1024*1024);
        m_framerate = total_frames*1000.0/time;
    }

    if (datalen)
    {
        m_stat[m_current_stat].frames++;
        m_stat[m_current_stat].total_data += datalen;
    }

}

float QnMediaStreamStatistics::getBitrateMbps() const
{
    QnMutexLocker locker( &m_mutex );
    return m_bitrateMbps;
}

float QnMediaStreamStatistics::getFrameRate() const
{
    QnMutexLocker locker( &m_mutex );
    return m_framerate;
}

int QnMediaStreamStatistics::getFrameSize() const
{
    return getBitrateMbps()/8*1024 / getFrameRate();
}

float QnMediaStreamStatistics::getAverageGopSize() const
{
    QnMutexLocker locker( &m_mutex );
    if ( m_keyFrames == 0 )
        return 0; // didn't have any frames yet

    return static_cast<float>( m_frames ) / static_cast<float>( m_keyFrames );
}

void QnMediaStreamStatistics::onBadSensor()
{
    if (!m_runing) return; 
    m_badsensor = true;
}
bool QnMediaStreamStatistics::badSensor() const
{
    return m_badsensor;
}

void QnMediaStreamStatistics::onEvent(QnMediaStreamStatisticsEvent event)
{
    if (!m_runing) return; 
    QnMutexLocker locker( &m_mutex );

    QDateTime current = QDateTime::currentDateTime();
    m_events[event].amount++;

    if (m_events[event].amount==1)
        m_events[event].firstTime = current;

    m_events[event].lastTime = current;

}

unsigned long QnMediaStreamStatistics::totalEvents(QnMediaStreamStatisticsEvent event) const
{
    QnMutexLocker locker( &m_mutex );
    return m_events[event].amount;
}

float QnMediaStreamStatistics::eventsPerHour(QnMediaStreamStatisticsEvent event) const
{
    QnMutexLocker locker( &m_mutex );
    QDateTime current = m_runing ? QDateTime::currentDateTime() : m_stopTime;

    unsigned long seconds = m_startTime.secsTo(current); 
    if (seconds==0)
        seconds = 1; // // to avoid devision by zero

    float hours = seconds/3600.0;
    return m_events[event].amount/hours;
}

QDateTime QnMediaStreamStatistics::firstOccurred(QnMediaStreamStatisticsEvent event) const
{
    QnMutexLocker locker( &m_mutex );
    return m_events[event].firstTime;
}

QDateTime QnMediaStreamStatistics::lastOccurred(QnMediaStreamStatisticsEvent event) const
{
    QnMutexLocker locker( &m_mutex );
    return m_events[event].lastTime;
}

float QnMediaStreamStatistics::getavBitrate() const
{
    QnMutexLocker locker( &m_mutex );
    QDateTime current = m_runing ? QDateTime::currentDateTime() : m_stopTime;
    unsigned long seconds = m_startTime.secsTo(current); 

    if (seconds==0)
        seconds = 1; // // to avoid devision by zero

    return (m_dataTotal*8.0)/(1024*1024)/seconds;

}

float QnMediaStreamStatistics::getavFrameRate() const
{
    QnMutexLocker locker( &m_mutex );
    QDateTime current = m_runing ? QDateTime::currentDateTime() : m_stopTime;
    unsigned long seconds = m_startTime.secsTo(current); 

    if (seconds==0)
        seconds = 1; // // to avoid devision by zero

    return (static_cast<float>(m_frames))/seconds;

}

unsigned long QnMediaStreamStatistics::totalSecs() const
{
    QnMutexLocker locker( &m_mutex );
    QDateTime current = m_runing ? QDateTime::currentDateTime() : m_stopTime;
    return m_startTime.secsTo(current); 

}

void QnMediaStreamStatistics::onLostConnection()
{
    QnMutexLocker locker( &m_mutex );
    m_connectionLost = true;
    m_connectionLostTime = QDateTime::currentDateTime();
}

bool QnMediaStreamStatistics::isConnectionLost() const
{
    return m_connectionLost;
}

int QnMediaStreamStatistics::connectionLostSec() const
{
    QnMutexLocker locker( &m_mutex );
    QDateTime current = QDateTime::currentDateTime();
    return m_connectionLostTime.secsTo(current); 
}
