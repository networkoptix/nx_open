#include "statistics.h"

#ifdef ENABLE_DATA_PROVIDERS

QnStatistics::QnStatistics()
{
    resetStatistics();
}

void QnStatistics::resetStatistics()
{
    SCOPED_MUTEX_LOCK( locker, &m_mutex);
    m_startTime = QDateTime::currentDateTime();
    m_frames = 0;
    m_dataTotal = 0;
    m_badsensor = false;

    for (int i = 0; i < CL_STAT_END; ++i)
        m_events[i].ammount = 0;

    for (int i = 0; i < CL_STATS_NUM; ++i)
    {
        m_stat[i].frames = 0;
        m_stat[i].totaldata = 0;
    }

    m_current_stat = 0;

    m_bitrate = 0;
    m_framerate = 0;

    m_runing = true;

    m_first_ondata_call = true;

    m_connectionLost = false;

}

void QnStatistics::stop()
{
    if (!m_runing) return; 
    SCOPED_MUTEX_LOCK( locker, &m_mutex);
    m_stopTime = QDateTime::currentDateTime();
    m_runing = false;
}

void QnStatistics::onData(unsigned int datalen)
{
    if (!m_runing) return; 
    SCOPED_MUTEX_LOCK( locker, &m_mutex);

    if (datalen)
    {
        ++m_frames;
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
            m_stat[index].totaldata = 0;
        }

        m_current_stat = (m_current_stat + ststat_num)%CL_STATS_NUM;

        // need to recalculate new bitrate&framerate

        int total_frames = 0;
        int total_bytes = 0;
        for(int i = 0; i < CL_STATS_NUM; ++i)
        {
            if (i!=m_current_stat)
            {
                total_bytes += m_stat[i].totaldata;
                total_frames += m_stat[i].frames;
            }
        }

        int time = (CL_STATS_NUM-1)*CL_STATISTICS_UPDATE_PERIOD_MS;

        m_bitrate = total_bytes*8*1000.0/time/(1024*1024);
        m_framerate = total_frames*1000.0/time;
    }

    if (datalen)
    {
        m_stat[m_current_stat].frames++;
        m_stat[m_current_stat].totaldata += datalen;
    }

}

float QnStatistics::getBitrate() const
{
    SCOPED_MUTEX_LOCK( locker, &m_mutex);
    return m_bitrate;
}

float QnStatistics::getFrameRate() const
{
    SCOPED_MUTEX_LOCK( locker, &m_mutex);
    return m_framerate;
}

int QnStatistics::getFrameSize() const
{
    return getBitrate()/8*1024 / getFrameRate();
}

void QnStatistics::onBadSensor()
{
    if (!m_runing) return; 
    m_badsensor = true;
}
bool QnStatistics::badSensor() const
{
    return m_badsensor;
}

void QnStatistics::onEvent(QnStatisticsEvent event)
{
    if (!m_runing) return; 
    SCOPED_MUTEX_LOCK( locker, &m_mutex);

    QDateTime current = QDateTime::currentDateTime();
    m_events[event].ammount++;

    if (m_events[event].ammount==1)
        m_events[event].firtTime = current;

    m_events[event].lastTime = current;

}

unsigned long QnStatistics::totalEvents(QnStatisticsEvent event) const
{
    SCOPED_MUTEX_LOCK( locker, &m_mutex);
    return m_events[event].ammount;
}

float QnStatistics::eventsPerHour(QnStatisticsEvent event) const
{
    SCOPED_MUTEX_LOCK( locker, &m_mutex);
    QDateTime current = m_runing ? QDateTime::currentDateTime() : m_stopTime;

    unsigned long seconds = m_startTime.secsTo(current); 
    if (seconds==0)
        seconds = 1; // // to avoid devision by zero

    float hours = seconds/3600.0;
    return m_events[event].ammount/hours;
}

QDateTime QnStatistics::firstOccurred(QnStatisticsEvent event) const
{
    SCOPED_MUTEX_LOCK( locker, &m_mutex);
    return m_events[event].firtTime;
}

QDateTime QnStatistics::lastOccurred(QnStatisticsEvent event) const
{
    SCOPED_MUTEX_LOCK( locker, &m_mutex);
    return m_events[event].lastTime;
}

float QnStatistics::getavBitrate() const
{
    SCOPED_MUTEX_LOCK( locker, &m_mutex);
    QDateTime current = m_runing ? QDateTime::currentDateTime() : m_stopTime;
    unsigned long seconds = m_startTime.secsTo(current); 

    if (seconds==0)
        seconds = 1; // // to avoid devision by zero

    return (m_dataTotal*8.0)/(1024*1024)/seconds;

}

float QnStatistics::getavFrameRate() const
{
    SCOPED_MUTEX_LOCK( locker, &m_mutex);
    QDateTime current = m_runing ? QDateTime::currentDateTime() : m_stopTime;
    unsigned long seconds = m_startTime.secsTo(current); 

    if (seconds==0)
        seconds = 1; // // to avoid devision by zero

    return (static_cast<float>(m_frames))/seconds;

}

unsigned long QnStatistics::totalSecs() const
{
    SCOPED_MUTEX_LOCK( locker, &m_mutex);
    QDateTime current = m_runing ? QDateTime::currentDateTime() : m_stopTime;
    return m_startTime.secsTo(current); 

}

void QnStatistics::onLostConnection()
{
    SCOPED_MUTEX_LOCK( locker, &m_mutex);
    m_connectionLost = true;
    m_connectionLostTime = QDateTime::currentDateTime();
}

bool QnStatistics::isConnectionLost() const
{
    return m_connectionLost;
}

int QnStatistics::connectionLostSec() const
{
    SCOPED_MUTEX_LOCK( locker, &m_mutex);
    QDateTime current = QDateTime::currentDateTime();
    return m_connectionLostTime.secsTo(current); 
}

#endif // ENABLE_DATA_PROVIDERS

