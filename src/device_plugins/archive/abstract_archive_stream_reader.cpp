#include "abstract_archive_stream_reader.h"


CLAbstractArchiveReader::CLAbstractArchiveReader(CLDevice* dev ):
CLClientPullStreamreader(dev),
mSingleShot(false),
mForward(true),
m_len_mksec(0),
m_need_tosleep(0),
m_cs(QMutex::Recursive),
mAdaptiveSleep(20*1000),
m_use_twice(false)
{
}

CLAbstractArchiveReader::~CLAbstractArchiveReader()
{

}

void CLAbstractArchiveReader::setSingleShotMode(bool single)
{
	mSingleShot = single;
	if (!mSingleShot)
	{
		mAdaptiveSleep.afterdelay();
		resume();
	}
}

bool CLAbstractArchiveReader::isSingleShotMode() const
{
	return mSingleShot;
}

void CLAbstractArchiveReader::setdirection(bool forward)
{
	mForward = forward;
}

bool CLAbstractArchiveReader::isForwardDirection() const
{
	return mForward;
}

// returns len of archive in mksec
quint64 CLAbstractArchiveReader::len_mks() const
{
	return m_len_mksec;
}


void CLAbstractArchiveReader::jumpTo(quint64 mksec, bool makeshot)
{
	QMutexLocker mutex(&m_cs);

	for (int channel = 0; channel  < m_channel_number; ++channel)
	{
		channeljumpTo(mksec, channel);
	}

	m_use_twice = true;

	if (makeshot && isSingleShotMode())
		resume();

}


