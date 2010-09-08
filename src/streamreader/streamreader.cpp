#include "streamreader.h"
#include "../base/log.h"



CLStreamreader::CLStreamreader(CLDevice* dev):
m_restartInfo(0),
m_restartHandler(0),
m_device(dev),
m_qulity(StreamQuality::CLSLowest)
{
}

CLStreamreader::~CLStreamreader()
{
	stop();
}

void CLStreamreader::setStatistics(CLStatistics* stat)
{
	m_stat = stat;
}

void CLStreamreader::setQuality(StreamQuality q)
{
	m_qulity = q;
}

CLStreamreader::StreamQuality CLStreamreader::getQuality() const
{
	return m_qulity;
}


void CLStreamreader::setStreamParams(CLParamList newParam)
{
	QMutexLocker mutex(&m_params_CS);
	m_streamParam = newParam;
}	

CLParamList CLStreamreader::getStreamParam() const
{
	QMutexLocker mutex(&m_params_CS);
	return m_streamParam;
}


void CLStreamreader::addDataProcessor(CLAbstractDataProcessor* dp)
{
	QMutexLocker mutex(&m_proc_CS);
	m_dataprocessors.push_back(dp);
}

void CLStreamreader::removeDataProcessor(CLAbstractDataProcessor* dp)
{
	QMutexLocker mutex(&m_proc_CS);
	m_dataprocessors.removeOne(dp);
}

void CLStreamreader::needKeyData()
{
	m_needKeyData = true;
}

void CLStreamreader::setDeviceRestartHadlerInfo(CLRestartHadlerInfo hinfo)
{
	m_restartInfo = hinfo;
}

void CLStreamreader::setDeviceRestartHadler(CLDeviceRestartHadler* restartHandler)
{
	m_restartHandler = restartHandler;
}

void CLStreamreader::putData(CLAbstractData* data)
{
	QMutexLocker mutex(&m_proc_CS);
	for (int i = 0; i < m_dataprocessors.size(); ++i)
	{
		CLAbstractDataProcessor* dp = m_dataprocessors.at(i);
		dp->putData(data);
	}

	data->releaseRef(); // now data belongs only to processors;
}