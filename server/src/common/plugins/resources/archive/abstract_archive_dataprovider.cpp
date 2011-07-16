#include "abstract_archive_dataprovider.h"


QnPlAbstractArchiveProvider::QnPlAbstractArchiveProvider(QnResource* dev )
    : CLClientPullStreamreader(dev),
      m_singleShot(false),
      m_forward(true),
      m_lengthMksec(0),
      m_startMksec(0),
      m_needToSleep(0),
      m_cs(QMutex::Recursive),
      m_adaptiveSleep(20 * 1000),
      m_useTwice(false),
	  m_skipFramesToTime(0)
{
}

QnPlAbstractArchiveProvider::~QnPlAbstractArchiveProvider()
{
}

void QnPlAbstractArchiveProvider::setSingleShotMode(bool single)
{
	m_singleShot = single;
	if (!m_singleShot)
	{
		m_adaptiveSleep.afterdelay();
		resume();
	}
}

bool QnPlAbstractArchiveProvider::isSingleShotMode() const
{
	return m_singleShot;
}

void QnPlAbstractArchiveProvider::setdirection(bool forward)
{
	m_forward = forward;
}

bool QnPlAbstractArchiveProvider::isForwardDirection() const
{
	return m_forward;
}

// returns len of archive in mksec
quint64 QnPlAbstractArchiveProvider::lengthMksec() const
{
	return m_lengthMksec;
}

quint64 QnPlAbstractArchiveProvider::startMksec() const
{
    return m_startMksec;
}

void QnPlAbstractArchiveProvider::jumpTo(quint64 mksec, bool makeshot)
{
 	QMutexLocker mutex(&m_cs);

    nextFrame();

	for (int channel = 0; channel  < m_NumaberOfVideoChannels; ++channel)
		channeljumpTo(mksec, channel);

	m_useTwice = true;

	if (makeshot && isSingleShotMode())
		resume();
}

void QnPlAbstractArchiveProvider::jumpToPreviousFrame(quint64 mksec, bool makeshot)
{
    setSkipFramesToTime(mksec);

	jumpTo(mksec - 100 * 1000, makeshot);
}

quint64 QnPlAbstractArchiveProvider::skipFramesToTime() const
{
    QMutexLocker mutex(&m_cs);

    return m_skipFramesToTime;
}

void QnPlAbstractArchiveProvider::setSkipFramesToTime(quint64 skipFramesToTime)
{
    QMutexLocker mutex(&m_cs);

    m_skipFramesToTime = skipFramesToTime;
}

bool QnPlAbstractArchiveProvider::isSkippingFrames() const
{ 
    QMutexLocker mutex(&m_cs);

    return m_skipFramesToTime != 0;
}


QStringList QnPlAbstractArchiveProvider::getAudioTracksInfo() const
{
    QStringList result;
    result << "Track 1";
    result << "Track 2";
    return result;
}