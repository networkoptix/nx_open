#include "abstract_streamdataprovider.h"


QnAbstractStreamDataProvider::QnAbstractStreamDataProvider(QnResourcePtr resource):
    QnResourceConsumer(resource),
    m_proc_CS(QMutex::Recursive),
    m_params_CS(QMutex::Recursive)
//m_needSleep(true)
{

}

QnAbstractStreamDataProvider::~QnAbstractStreamDataProvider()
{
	stop();
}

bool QnAbstractStreamDataProvider::dataCanBeAccepted() const
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


void QnAbstractStreamDataProvider::addDataProcessor(QnAbstractDataConsumer* dp)
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

void QnAbstractStreamDataProvider::removeDataProcessor(QnAbstractDataConsumer* dp)
{
	QMutexLocker mutex(&m_proc_CS);
	m_dataprocessors.removeOne(dp);
}

void QnAbstractStreamDataProvider::setSpeed(double value)
{
    QMutexLocker mutex(&m_proc_CS);
    for (int i = 0; i < m_dataprocessors.size(); ++i)
    {
        QnAbstractDataConsumer* dp = m_dataprocessors.at(i);
        dp->setSpeed(value);
    }
    setReverseMode(value < 0);
}

void QnAbstractStreamDataProvider::putData(QnAbstractDataPacketPtr data)
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

bool QnAbstractStreamDataProvider::isConnectedToTheResource() const
{
    return m_resource->hasSuchConsumer(const_cast<QnAbstractStreamDataProvider*>(this));
}

void QnAbstractStreamDataProvider::disconnectFromResource()
{
    stop();
    
    QnResourceConsumer::disconnectFromResource();
}

void QnAbstractStreamDataProvider::beforeDisconnectFromResource()
{
    pleaseStop();
}
