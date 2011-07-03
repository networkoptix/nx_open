#include "resource/resource.h"
#include "streamdataprovider.h"
#include "resource/video_resource_layout.h"


QnStreamDataProvider::QnStreamDataProvider(QnResource* dev):
m_device(dev),
m_qulity(CLSLowest),
m_needSleep(true)
{
	memset(m_gotKeyFrame, 0, sizeof(m_gotKeyFrame));
	m_channel_number = dev->getVideoLayout()->numberOfChannels();

	setStreamParams(m_device->getStreamParamList());

}

QnStreamDataProvider::~QnStreamDataProvider()
{
	stop();
}

QnResource* QnStreamDataProvider::getDevice() const
{
	return m_device;
}

bool QnStreamDataProvider::dataCanBeAccepted() const
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



void QnStreamDataProvider::setStatistics(CLStatistics* stat)
{
	m_stat = stat;
}

void QnStreamDataProvider::setQuality(StreamQuality q)
{
	m_qulity = q;
}

QnStreamDataProvider::StreamQuality QnStreamDataProvider::getQuality() const
{
	return m_qulity;
}

void QnStreamDataProvider::setStreamParams(QnParamList newParam)
{
	QMutexLocker mutex(&m_params_CS);
	m_streamParam = newParam;
}	

QnParamList QnStreamDataProvider::getStreamParam() const
{
	QMutexLocker mutex(&m_params_CS);
	return m_streamParam;
}

void QnStreamDataProvider::addDataProcessor(QnAbstractDataConsumer* dp)
{
	QMutexLocker mutex(&m_proc_CS);

	if (!m_dataprocessors.contains(dp))
		m_dataprocessors.push_back(dp);
}

void QnStreamDataProvider::removeDataProcessor(QnAbstractDataConsumer* dp)
{
	QMutexLocker mutex(&m_proc_CS);
	m_dataprocessors.removeOne(dp);
}

void QnStreamDataProvider::setNeedKeyData()
{
	for (int i = 0; i < m_channel_number; ++i)
		m_gotKeyFrame[i] = 0;
}

bool QnStreamDataProvider::needKeyData(int channel) const
{
	return m_gotKeyFrame[channel]==0;
}

bool QnStreamDataProvider::needKeyData() const
{
	for (int i = 0; i < m_channel_number; ++i)
		if (m_gotKeyFrame[i]==0)
			return true;

	return false;
}

void QnStreamDataProvider::pauseDataProcessors()
{
    foreach(QnAbstractDataConsumer* dataProcessor, m_dataprocessors) 
    {
        dataProcessor->pause();
    }
}

void QnStreamDataProvider::resumeDataProcessors()
{
    foreach(QnAbstractDataConsumer* dataProcessor, m_dataprocessors) 
    {
        dataProcessor->resume();
    }
}


void QnStreamDataProvider::putData(QnAbstractDataPacketPtr data)
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

void QnStreamDataProvider::setNeedSleep(bool sleep)
{
    m_needSleep = sleep;   
}
