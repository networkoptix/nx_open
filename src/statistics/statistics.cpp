#include "statistics.h"

CLStatistics::CLStatistics()
{
	resetStatistics();
}

void CLStatistics::resetStatistics()
{
	QMutexLocker locker(&m_mutex);
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

void CLStatistics::stop()
{
	if (!m_runing) return; 
	QMutexLocker locker(&m_mutex);
	m_stopTime = QDateTime::currentDateTime();
	m_runing = false;
}

void CLStatistics::onData(unsigned int datalen)
{
	if (!m_runing) return; 
	QMutexLocker locker(&m_mutex);

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

float CLStatistics::getBitrate() const
{
	QMutexLocker locker(&m_mutex);
	return m_bitrate;
}

float CLStatistics::getFrameRate() const
{
	QMutexLocker locker(&m_mutex);
	return m_framerate;
}

void CLStatistics::onBadSensor()
{
	if (!m_runing) return; 
	m_badsensor = true;
}
bool CLStatistics::badSensor() const
{
	return m_badsensor;
}

void CLStatistics::onEvent(CLStatisticsEvent event)
{
	if (!m_runing) return; 
	QMutexLocker locker(&m_mutex);

	QDateTime current = QDateTime::currentDateTime();
	m_events[event].ammount++;

	if (m_events[event].ammount==1)
		m_events[event].firtTime = current;

	m_events[event].lastTime = current;

}

unsigned long CLStatistics::totalEvents(CLStatisticsEvent event) const
{
	QMutexLocker locker(&m_mutex);
	return m_events[event].ammount;
}

float CLStatistics::eventsPerHour(CLStatisticsEvent event) const
{
	QMutexLocker locker(&m_mutex);
	QDateTime current = m_runing ? QDateTime::currentDateTime() : m_stopTime;

	unsigned long seconds = m_startTime.secsTo(current); 
	if (seconds==0)
		seconds = 1; // // to avoid devision by zero

	float hours = seconds/3600.0;
	return m_events[event].ammount/hours;
}

QDateTime CLStatistics::firstOccurred(CLStatisticsEvent event) const
{
	QMutexLocker locker(&m_mutex);
	return m_events[event].firtTime;
}

QDateTime CLStatistics::lastOccurred(CLStatisticsEvent event) const
{
	QMutexLocker locker(&m_mutex);
	return m_events[event].lastTime;
}

float CLStatistics::getavBitrate() const
{
	QMutexLocker locker(&m_mutex);
	QDateTime current = m_runing ? QDateTime::currentDateTime() : m_stopTime;
	unsigned long seconds = m_startTime.secsTo(current); 

	if (seconds==0)
		seconds = 1; // // to avoid devision by zero

	return (m_dataTotal*8.0)/(1024*1024)/seconds;

}

float CLStatistics::getavFrameRate() const
{
	QMutexLocker locker(&m_mutex);
	QDateTime current = m_runing ? QDateTime::currentDateTime() : m_stopTime;
	unsigned long seconds = m_startTime.secsTo(current); 

	if (seconds==0)
		seconds = 1; // // to avoid devision by zero

	return (static_cast<float>(m_frames))/seconds;

}

unsigned long CLStatistics::totalSecs() const
{
	QMutexLocker locker(&m_mutex);
	QDateTime current = m_runing ? QDateTime::currentDateTime() : m_stopTime;
	return m_startTime.secsTo(current); 

}

void CLStatistics::onLostConnection()
{
	QMutexLocker locker(&m_mutex);
	m_connectionLost = true;
	m_connectionLostTime = QDateTime::currentDateTime();
}

bool CLStatistics::isConnectioLost() const
{
	return m_connectionLost;
}

int CLStatistics::connectionLostSec() const
{
	QMutexLocker locker(&m_mutex);
	QDateTime current = QDateTime::currentDateTime();
	return m_connectionLostTime.secsTo(current); 
}
