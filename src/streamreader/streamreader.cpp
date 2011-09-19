#include "streamreader.h"
#include "../base/log.h"
#include "device/device.h"
#include "device/device_video_layout.h"

CLStreamreader::CLStreamreader(CLDevice* dev):
m_device(dev),
m_qulity(CLSLowest)
//m_needSleep(true)
{
	memset(m_gotKeyFrame, 0, sizeof(m_gotKeyFrame));
	m_channel_number = dev->getVideoLayout(0)->numberOfChannels();

	setStreamParams(m_device->getStreamParamList());

}

CLStreamreader::~CLStreamreader()
{
	stop();
}

CLDevice* CLStreamreader::getDevice() const
{
	return m_device;
}

bool CLStreamreader::dataCanBeAccepted() const
{
    // need to read only if all queues has more space and at least one queue is exist
    bool result = false;
    for (int i = 0; i < m_dataprocessors.size(); ++i)
    {
        CLAbstractDataProcessor* dp = m_dataprocessors.at(i);

        if (dp->canAcceptData())
            result = true;
        else 
            return false;
    }

    return result;
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

	if (!m_dataprocessors.contains(dp))
    {
		m_dataprocessors.push_back(dp);
        connect(this, SIGNAL(audioParamsChanged(AVCodecContext*)), dp, SLOT(onAudioParamsChanged(AVCodecContext*)), Qt::DirectConnection);
        connect(this, SIGNAL(realTimeStreamHint(bool)), dp, SLOT(onRealTimeStreamHint(bool)), Qt::DirectConnection);
        connect(this, SIGNAL(slowSourceHint()), dp, SLOT(onSlowSourceHint()), Qt::DirectConnection);
    }
}

void CLStreamreader::removeDataProcessor(CLAbstractDataProcessor* dp)
{
	QMutexLocker mutex(&m_proc_CS);
	m_dataprocessors.removeOne(dp);
}

void CLStreamreader::setNeedKeyData()
{
	for (int i = 0; i < m_channel_number; ++i)
		m_gotKeyFrame[i] = 0;
}

bool CLStreamreader::needKeyData(int channel) const
{
	return m_gotKeyFrame[channel]==0;
}

bool CLStreamreader::needKeyData() const
{
	for (int i = 0; i < m_channel_number; ++i)
		if (m_gotKeyFrame[i]==0)
			return true;

	return false;

}

void CLStreamreader::setSpeed(double value)
{
    QMutexLocker mutex(&m_proc_CS);
    for (int i = 0; i < m_dataprocessors.size(); ++i)
    {
        CLAbstractDataProcessor* dp = m_dataprocessors.at(i);
        dp->setSpeed(value);
    }
    setReverseMode(value < 0);
}

void CLStreamreader::putData(CLAbstractData* data)
{
    if (!data)
        return;

	QMutexLocker mutex(&m_proc_CS);
	for (int i = 0; i < m_dataprocessors.size(); ++i)
	{
		CLAbstractDataProcessor* dp = m_dataprocessors.at(i);
		dp->putData(data);
	}

	data->releaseRef(); // now data belongs only to processors;
}

/*
void CLStreamreader::setNeedSleep(bool sleep)
{
    m_needSleep = sleep;   
}
*/