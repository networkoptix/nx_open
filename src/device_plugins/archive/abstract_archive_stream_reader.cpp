#include "abstract_archive_stream_reader.h"


CLAbstractArchiveReader::CLAbstractArchiveReader(CLDevice* dev ):
CLClientPullStreamreader(dev),
mSingleShot(true),
mForward(true),
m_len_msec(0)
{

}

CLAbstractArchiveReader::~CLAbstractArchiveReader()
{

}

void CLAbstractArchiveReader::setSingleShotMode(bool single)
{
	mSingleShot = true;
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

unsigned int CLAbstractArchiveReader::nextChannel() const
{

	//=====find slowest channel ========
	int slowest_channel = 0;
	unsigned long best_slowest_time = mForward ? 0xffffffff : 0;
	for (int channel = 0; channel < m_channel_number; ++channel)
	{
		if (mMovie[channel].count()==0)
			continue;

		unsigned long curr_channel_time = mMovie[channel].at(mCurrIndex[channel]).time_ms;

		if ((mForward && curr_channel_time <= best_slowest_time) || (!mForward && curr_channel_time >= best_slowest_time))
		{
			best_slowest_time = curr_channel_time;
			slowest_channel = channel;
		}		
	}

	//===== so we've got slowest_channel and best_slowest_time
	int best_channel = 0;
	unsigned long best_diff = 0xffffffff;
	for (int channel = 0; channel < m_channel_number; ++channel)
	{
		int next_index = nextFrameIndex(channel, needKeyData(channel), mForward);
		if (next_index<0)
			continue;

		unsigned long curr_channel_next_time = mMovie[channel].at( next_index ).time_ms;

		int diff = labs(curr_channel_next_time - best_slowest_time);

		if (diff<best_diff)
		{
			best_diff = diff;
			best_channel = channel;
		}
	}

	return best_channel;

}

void CLAbstractArchiveReader::jumpTo(unsigned long msec, bool makeshot)
{

}


void CLAbstractArchiveReader::jumpTo(unsigned long msec, int channel)
{

}

int CLAbstractArchiveReader::nextFrameIndex(int channel, bool keyframe, bool forwarddirection) const
{
	int curr_index = mCurrIndex[channel];

	while(1)
	{
		curr_index = forwarddirection ? curr_index+1 : curr_index-1;
		if (curr_index<0 || curr_index> mMovie[channel].count())
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

	int index1 = 0;
	int index2 = mMovie[channel].count();

	while(index1!=index2)
	{

	}


	return index1;
}