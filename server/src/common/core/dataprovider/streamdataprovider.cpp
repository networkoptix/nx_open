#include "resource/resource.h"
#include "streamdataprovider.h"
#include "resource/video_resource_layout.h"


QnMediaStreamDataProvider::QnMediaStreamDataProvider(QnResource* dev):
m_device(dev),
m_qulity(CLSLowest),
m_needSleep(true)
{
	memset(m_gotKeyFrame, 0, sizeof(m_gotKeyFrame));
	m_channel_number = dev->getVideoLayout()->numberOfChannels();

	setStreamParams(m_device->getStreamParamList());

}

QnMediaStreamDataProvider::~QnMediaStreamDataProvider()
{
	stop();
}

QnResource* QnMediaStreamDataProvider::getDevice() const
{
	return m_device;
}

bool QnMediaStreamDataProvider::dataCanBeAccepted() const
{
    // need to read only if all queues has more space and at least one queue is exist
    bool result = false;
    for (int i = 0; i < m_dataprocessors.size(); ++i)
    {
        QnAbstractDataConsumer* dp = m_dataprocessors.at(i);

        if (dp->canAcceptData())
            result = true;
        else 
            return false;
    }

    return result;
}



void QnMediaStreamDataProvider::setStatistics(QnStatistics* stat)
{
	m_stat = stat;
}

void QnMediaStreamDataProvider::setQuality(StreamQuality q)
{
	m_qulity = q;
}

QnMediaStreamDataProvider::StreamQuality QnMediaStreamDataProvider::getQuality() const
{
	return m_qulity;
}

void QnMediaStreamDataProvider::setStreamParams(QnParamList newParam)
{
	QMutexLocker mutex(&m_params_CS);
	m_streamParam = newParam;
}	

QnParamList QnMediaStreamDataProvider::getStreamParam() const
{
	QMutexLocker mutex(&m_params_CS);
	return m_streamParam;
}

void QnMediaStreamDataProvider::addDataProcessor(QnAbstractDataConsumer* dp)
{
	QMutexLocker mutex(&m_proc_CS);

	if (!m_dataprocessors.contains(dp))
		m_dataprocessors.push_back(dp);
}

void QnMediaStreamDataProvider::removeDataProcessor(QnAbstractDataConsumer* dp)
{
	QMutexLocker mutex(&m_proc_CS);
	m_dataprocessors.removeOne(dp);
}

void QnMediaStreamDataProvider::setNeedKeyData()
{
	for (int i = 0; i < m_channel_number; ++i)
		m_gotKeyFrame[i] = 0;
}

bool QnMediaStreamDataProvider::needKeyData(int channel) const
{
	return m_gotKeyFrame[channel]==0;
}

bool QnMediaStreamDataProvider::needKeyData() const
{
	for (int i = 0; i < m_channel_number; ++i)
		if (m_gotKeyFrame[i]==0)
			return true;

	return false;
}

void QnMediaStreamDataProvider::pauseDataProcessors()
{
    foreach(QnAbstractDataConsumer* dataProcessor, m_dataprocessors) 
    {
        dataProcessor->pause();
    }
}

void QnMediaStreamDataProvider::resumeDataProcessors()
{
    foreach(QnAbstractDataConsumer* dataProcessor, m_dataprocessors) 
    {
        dataProcessor->resume();
    }
}


void QnMediaStreamDataProvider::putData(QnAbstractDataPacketPtr data)
{
    if (!data)
        return;

	QMutexLocker mutex(&m_proc_CS);
	for (int i = 0; i < m_dataprocessors.size(); ++i)
	{
		QnAbstractDataConsumer* dp = m_dataprocessors.at(i);
		dp->putData(data);
	}
}

void QnMediaStreamDataProvider::setNeedSleep(bool sleep)
{
    m_needSleep = sleep;   
}
