#include "abstract_archive_stream_reader.h"

CLAbstractArchiveReader::CLAbstractArchiveReader(CLDevice* dev )
    : CLClientPullStreamreader(dev),
      m_singleShot(false),
      m_forward(true),
      m_lengthMksec(0),
      m_needToSleep(0),
      m_cs(QMutex::Recursive),
      m_adaptiveSleep(20 * 1000),
      m_useTwice(false)
{
}

CLAbstractArchiveReader::~CLAbstractArchiveReader()
{
}

void CLAbstractArchiveReader::setSingleShotMode(bool single)
{
	m_singleShot = single;
	if (!m_singleShot)
	{
		m_adaptiveSleep.afterdelay();
		resume();
	}
}

bool CLAbstractArchiveReader::isSingleShotMode() const
{
	return m_singleShot;
}

void CLAbstractArchiveReader::setdirection(bool forward)
{
	m_forward = forward;
}

bool CLAbstractArchiveReader::isForwardDirection() const
{
	return m_forward;
}

// returns len of archive in mksec
quint64 CLAbstractArchiveReader::lengthMksec() const
{
	return m_lengthMksec;
}

void CLAbstractArchiveReader::jumpTo(quint64 mksec, bool makeshot)
{
	QMutexLocker mutex(&m_cs);

	for (int channel = 0; channel  < m_channel_number; ++channel)
	{
		channeljumpTo(mksec, channel);
	}

	m_useTwice = true;

	if (makeshot && isSingleShotMode())
		resume();

}

