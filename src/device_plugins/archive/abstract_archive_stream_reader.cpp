#include "abstract_archive_stream_reader.h"


CLAbstractArchiveReader::CLAbstractArchiveReader(CLDevice* dev ):
CLClientPullStreamreader(dev),
mSingleShot(false),
mForward(true),
m_len_msec(0),
m_need_tosleep(0),
m_cs(QMutex::Recursive)
{
	memset(mCurrIndex,0,sizeof(mCurrIndex));
	for (int channel = 0; channel < m_channel_number; ++channel)
	{
		mFinished[channel] = false;
	}
}

CLAbstractArchiveReader::~CLAbstractArchiveReader()
{

}

void CLAbstractArchiveReader::setSingleShotMode(bool single)
{
	mSingleShot = single;
	if (!mSingleShot)
		mAdaptiveSleep.afterdelay();
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

// returns len of archive in msec
unsigned long CLAbstractArchiveReader::len_msec() const
{
	return m_len_msec;
}

unsigned long CLAbstractArchiveReader::currTime() const
{
	QMutexLocker mutex(&m_cs);
	int slowe_channel = slowest_channel();
	return mMovie[slowe_channel].at(mCurrIndex[slowe_channel]).time_ms;
}

bool CLAbstractArchiveReader::reachedTheEnd() const
{
	for (int channel = 0; channel < m_channel_number; ++channel)
	{
		if (!mFinished[channel])
			return false;
	}

	return true;

}


void CLAbstractArchiveReader::jumpTo(unsigned long msec, bool makeshot)
{
	QMutexLocker mutex(&m_cs);

	for (int channel = 0; channel  < m_channel_number; ++channel)
	{
		channeljumpTo(msec, channel);
	}

	

	if (makeshot && isSingleShotMode())
		resume();
}


void CLAbstractArchiveReader::channeljumpTo(unsigned long msec, int channel)
{
	int new_index = findBestIndex(channel, msec);
	if (new_index<0)
		return;

	mCurrIndex[channel] = nextFrameIndex(true, channel, new_index, true, mForward);
	mFinished[channel] = (mCurrIndex[channel]==-1);
	
}

int CLAbstractArchiveReader::nextFrameIndex(bool after_jump, int channel, int curr_index , bool keyframe, bool forwarddirection) const
{
	while(1)
	{
		if (!after_jump)
			curr_index = forwarddirection ? curr_index+1 : curr_index-1;

		after_jump = false;

		if (curr_index<0 || curr_index>=mMovie[channel].count())
			return -1;

		if (!keyframe)
			return curr_index; // everything works

		const ArchiveFrameInfo& finfo = mMovie[channel].at(curr_index);
		if (finfo.keyFrame)
			return curr_index;
	}
}

int CLAbstractArchiveReader::findBestIndex(int channel, unsigned long msec) const
{

	if (mMovie[channel].count()==0)
		return -1;


	int index1 = 0;
	int index2 = mMovie[channel].count()-1;

	unsigned long index1_time = mMovie[channel].at(index1).time_ms;
	unsigned long index2_time = mMovie[channel].at(index2).time_ms;
	if (msec >= index2_time)
		return index2;

	if (msec <= index1_time)
		return index1;


	while(index1!=index2)
	{
		int new_index = (index1+index2)/2;
		unsigned long new_index_time = mMovie[channel].at(new_index_time).time_ms;

		if (new_index_time>=msec)
			index2 = new_index;
		else
			index1 = new_index;
	}


	return index1;
}

int CLAbstractArchiveReader::slowest_channel() const
{
	//=====find slowest channel ========
	int slowest_channel = -1;
	unsigned long best_slowest_time = mForward ? 0xffffffff : 0;
	for (int channel = 0; channel < m_channel_number; ++channel)
	{

		if (mFinished[channel])
			continue;

		if (mMovie[channel].count()==0)
			continue;

		unsigned long curr_channel_time = mMovie[channel].at(mCurrIndex[channel]).time_ms;

		if ((mForward && curr_channel_time <= best_slowest_time) || (!mForward && curr_channel_time >= best_slowest_time))
		{
			best_slowest_time = curr_channel_time;
			slowest_channel = channel;
		}		
	}

	return slowest_channel;

}