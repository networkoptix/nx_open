#include "resource/resource.h"
#include "streamdataprovider.h"
#include "resource/video_resource_layout.h"


QnMediaStreamDataProvider::QnMediaStreamDataProvider(QnResourcePtr res):
QnAbstractStreamDataProvider(res),
m_qulity(CLSLowest)

{
	memset(m_gotKeyFrame, 0, sizeof(m_gotKeyFrame));
	m_NumaberOfVideoChannels = 0;//dev->getVideoLayout()->numberOfChannels();

}

QnMediaStreamDataProvider::~QnMediaStreamDataProvider()
{
	stop();
}



void QnMediaStreamDataProvider::setQuality(StreamQuality q)
{
    QMutexLocker mtx(&m_mtx);
	m_qulity = q;
}

QnMediaStreamDataProvider::StreamQuality QnMediaStreamDataProvider::getQuality() const
{
    QMutexLocker mtx(&m_mtx);
	return m_qulity;
}

void QnMediaStreamDataProvider::setNeedKeyData()
{
	for (int i = 0; i < m_NumaberOfVideoChannels; ++i)
		m_gotKeyFrame[i] = 0;
}

bool QnMediaStreamDataProvider::needKeyData(int channel) const
{
	return m_gotKeyFrame[channel]==0;
}

bool QnMediaStreamDataProvider::needKeyData() const
{
	for (int i = 0; i < m_NumaberOfVideoChannels; ++i)
		if (m_gotKeyFrame[i]==0)
			return true;

	return false;
}

