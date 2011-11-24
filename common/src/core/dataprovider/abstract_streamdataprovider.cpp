#include "abstract_streamdataprovider.h"
#include "../resource/resource.h"


QnAbstractStreamDataProvider::QnAbstractStreamDataProvider(QnResourcePtr resource):
    QnResourceConsumer(resource),
    m_mutex(QMutex::Recursive)
{
    m_speed = 1.0;
}

QnAbstractStreamDataProvider::~QnAbstractStreamDataProvider()
{
	stop();
}

bool QnAbstractStreamDataProvider::dataCanBeAccepted() const
{
    // need to read only if all queues has more space and at least one queue is exist
    QMutexLocker mutex(&m_mutex);
    for (int i = 0; i < m_dataprocessors.size(); ++i)
    {
        QnAbstractDataConsumer* dp = m_dataprocessors.at(i);
        if (!dp->canAcceptData())
            return false;
    }
    return true;
}


void QnAbstractStreamDataProvider::addDataProcessor(QnAbstractDataConsumer* dp)
{
	QMutexLocker mutex(&m_mutex);

	if (!m_dataprocessors.contains(dp))
    {
		m_dataprocessors.push_back(dp);
        connect(this, SIGNAL(audioParamsChanged(AVCodecContext*)), dp, SLOT(onAudioParamsChanged(AVCodecContext*)), Qt::DirectConnection);
        connect(this, SIGNAL(realTimeStreamHint(bool)), dp, SLOT(onRealTimeStreamHint(bool)), Qt::DirectConnection);
        connect(this, SIGNAL(slowSourceHint()), dp, SLOT(onSlowSourceHint()), Qt::DirectConnection);

        connect(this, SIGNAL(beforeJump(qint64, bool)), dp, SLOT(onBeforeJump(qint64, bool)), Qt::DirectConnection);
        connect(this, SIGNAL(jumpOccured(qint64)), dp, SLOT(onJumpOccured(qint64)), Qt::DirectConnection);
        connect(this, SIGNAL(prevFrameOccured()), dp, SLOT(onPrevFrameOccured()), Qt::DirectConnection);
        connect(this, SIGNAL(nextFrameOccured()), dp, SLOT(onNextFrameOccured()), Qt::DirectConnection);

    }
}

void QnAbstractStreamDataProvider::removeDataProcessor(QnAbstractDataConsumer* dp)
{
	QMutexLocker mutex(&m_mutex);
	m_dataprocessors.removeOne(dp);
}

double QnAbstractStreamDataProvider::getSpeed() const
{
    QMutexLocker mutex(&m_mutex);
    return m_speed;
}

void QnAbstractStreamDataProvider::setSpeed(double value)
{
    QMutexLocker mutex(&m_mutex);
    m_speed = value;
    for (int i = 0; i < m_dataprocessors.size(); ++i)
    {
        QnAbstractDataConsumer* dp = m_dataprocessors.at(i);
        dp->setSpeed(value);
    }
    setReverseMode(value < 0);
    emit speedChanged(value);
}

void QnAbstractStreamDataProvider::putData(QnAbstractDataPacketPtr data)
{
    if (!data)
        return;

	QMutexLocker mutex(&m_mutex);
	for (int i = 0; i < m_dataprocessors.size(); ++i)
	{
		QnAbstractDataConsumer* dp = m_dataprocessors.at(i);
		dp->putData(data);
	}
}

bool QnAbstractStreamDataProvider::isConnectedToTheResource() const
{
    return m_resource->hasSuchConsumer(const_cast<QnAbstractStreamDataProvider*>(this));
}

void QnAbstractStreamDataProvider::disconnectFromResource()
{
    stop();
    QnResourceConsumer::disconnectFromResource();
    //m_device->removeConsumer(this);
}

void QnAbstractStreamDataProvider::beforeDisconnectFromResource()
{
    pleaseStop();
}
