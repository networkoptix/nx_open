#include "archive_stream_reader.h"
#include "device\device.h"
#include "base\bytearray.h"
#include "data\mediadata.h"

CLArchiveStreamReader::CLArchiveStreamReader(CLDevice* dev):
CLAbstractArchiveReader(dev),
m_firsttime(true)

{
	memset(mCurrIndex,0,sizeof(mCurrIndex));
	for (int channel = 0; channel < m_channel_number; ++channel)
	{
		mFinished[channel] = false;
	}

	init_data();
}

CLArchiveStreamReader::~CLArchiveStreamReader()
{

}

void CLArchiveStreamReader::resume()
{
	CLAbstractArchiveReader::resume();
	m_adaptiveSleep.afterdelay();
}

void CLArchiveStreamReader::init_data()
{
	for (int channel = 0; channel < m_channel_number; ++channel)
	{
		QString fn;
		QTextStream stream(&fn);
		stream << m_device->getUniqueId() << "/channel_" << channel;
		QString descr_file_name = fn + ".descr";
		QString data_file_name = fn + ".data";

		QFile file(descr_file_name);
		if (!file.exists())
			continue;

		if (!file.open(QIODevice::ReadOnly))
			continue;

		int decr_size = file.size();

		CLByteArray data(0, decr_size);
		data.prepareToWrite(decr_size);
		QDataStream in(&file);
		int readed = in.readRawData(data.data(), decr_size);
		data.done(readed);
		if (readed<4)
			continue;

		m_data_file[channel].setFileName(data_file_name);
		if (!m_data_file[channel].exists())
			continue;

		if (!m_data_file[channel].open(QIODevice::ReadOnly))
			continue;

		m_data_stream[channel].setDevice(&m_data_file[channel]);

		unsigned int ver = *( (unsigned int*)(data.data()) ); // read first for bytes
		parse_channel_data(channel ,ver, data.data()+4, data.size()-4);
	}

	//=====choosing min time
	QDateTime min_date_time;
	bool first = true;
	for (int channel = 0; channel < m_channel_number; ++channel)
	{	
		if (mMovie[channel].count()==0)
			continue;

		if (first)
		{
			min_date_time = QDateTime::fromMSecsSinceEpoch(mMovie[channel].at(0).abs_time/1000);
			first = false;
		}

		QDateTime time = QDateTime::fromMSecsSinceEpoch(mMovie[channel].at(0).abs_time/1000);

		if (time<min_date_time)
			min_date_time = time;
	}

	//===========time_ms===================

	for (int channel = 0; channel < m_channel_number; ++channel)
	{
		for(int i = 0; i < mMovie[channel].count();++i)
		{
			ArchiveFrameInfo& info = mMovie[channel][i];

			QDateTime time = QDateTime::fromMSecsSinceEpoch(info.abs_time/1000);

			quint64 time_mks = min_date_time.msecsTo(time)*1000;
			info.time = time_mks;

			if (time_mks>m_lengthMksec)
				m_lengthMksec = time_mks;
		}
	}

}

quint64 CLArchiveStreamReader::currentTime() const
{
	QMutexLocker mutex(&m_cs);

    quint64 jumpTime = skipFramesToTime();
    if (jumpTime)
        return jumpTime;

	int slowe_channel = slowest_channel();
	return mMovie[slowe_channel].at(mCurrIndex[slowe_channel]).time;
}

CLAbstractMediaData* CLArchiveStreamReader::getNextData()
{
	if (m_firsttime)
	{
		//init_data();
		m_firsttime = false;
	}

	if (!isSingleShotMode() && !isSkippingFrames())
		m_adaptiveSleep.sleep(m_needToSleep);

	// will return next channel ( if channel is finished it will not be selected
	int channel;

	{
		QMutexLocker mutex(&m_cs);
		channel = slowest_channel(); 
	}

	if (channel<0)
	{
		// should not get here;
		// if we here => some data file corrupted
		msleep(20); // to avoid CPU load
		return 0;
	}

	QMutexLocker mutex(&m_cs);

	const ArchiveFrameInfo &finfo = mMovie[channel].at(mCurrIndex[channel]);

	CLCompressedVideoData* videoData = new CLCompressedVideoData(CL_MEDIA_ALIGNMENT,finfo.size);
	CLByteArray& data = videoData->data;

	data.prepareToWrite(finfo.size);
	int readed = m_data_stream[channel].readRawData(data.data(), finfo.size);
	data.done(readed);

	videoData->compressionType = CLCodecType(finfo.codec);
	videoData->keyFrame = finfo.keyFrame;
	videoData->channelNumber = channel;
	videoData->useTwice = m_useTwice;
	videoData->timestamp = finfo.time;

	m_useTwice = false;

	if (videoData->keyFrame)
		m_gotKeyFrame[channel] = true;

	//=================

	m_needToSleep = 0;

	// changing current index of this channel
	int new_index = nextFrameIndex(false, channel, mCurrIndex[channel], needKeyData(channel), m_forward);
	if (new_index<0)
	{
		mFinished[channel]=true; // slowest_channel will not return this channel any more 
		if (reachedTheEnd())
		{
			if (m_forward)
				jumpTo(0, false);
			else
				jumpTo(lengthMksec(), false);
		}
	}
	else
	{
		if (new_index - mCurrIndex[channel] !=1 ) // if need to seek file 
		{
			int shift = mMovie[channel].at(new_index).shift;
			m_data_file[channel].seek(shift);
		}

		qint64 this_time = mMovie[channel].at(mCurrIndex[channel]).time;
		mCurrIndex[channel] = new_index;

		int next_channel = slowest_channel();
		qint64 next_time = mMovie[channel].at(mCurrIndex[next_channel]).time;
        if (next_time < skipFramesToTime())
            videoData->ignore = true;
        else
            setSkipFramesToTime(0);

		m_needToSleep = labs(next_time - this_time);

	}

	//=================
	if (isSingleShotMode() && !isSkippingFrames())
		pause();

	return videoData;

}

//================================================================

bool CLArchiveStreamReader::reachedTheEnd() const
{
	for (int channel = 0; channel < m_channel_number; ++channel)
	{
		if (!mFinished[channel])
			return false;
	}

	return true;

}

int CLArchiveStreamReader::nextFrameIndex(bool after_jump, int channel, int curr_index , bool keyframe, bool forwarddirection) const
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

void CLArchiveStreamReader::channeljumpTo(quint64 mksec, int channel)
{

	int new_index = findBestIndex(channel, mksec);
	if (new_index<0)
		return;

	mCurrIndex[channel] = nextFrameIndex(true, channel, new_index, true, !m_forward);

	if (mCurrIndex[channel]==-1)
		mCurrIndex[channel] = nextFrameIndex(true, channel, new_index, true, m_forward);

	mFinished[channel] = (mCurrIndex[channel]==-1);

	if (mCurrIndex[channel]==-1)
		return;

	unsigned long shift = mMovie[channel].at(mCurrIndex[channel]).shift;
	m_data_file[channel].seek(shift);
}

int CLArchiveStreamReader::findBestIndex(int channel, quint64 mksec) const
{

	if (mMovie[channel].count()==0)
		return -1;

	int index1 = 0;
	int index2 = mMovie[channel].count()-1;

	quint64 index1_time = mMovie[channel].at(index1).time;
	quint64 index2_time = mMovie[channel].at(index2).time;
	if (mksec >= index2_time)
		return index2;

	if (mksec <= index1_time)
		return index1;

	while(index2 - index1 > 1)
	{
		int new_index = (index1+index2)/2;
		quint64 new_index_time = mMovie[channel].at(new_index).time;

		if (new_index_time>=mksec)
			index2 = new_index;
		else
			index1 = new_index;
	}

	return m_forward ? index1 : index2;
}

int CLArchiveStreamReader::slowest_channel() const
{
	//=====find slowest channel ========
	int slowest_channel = -1;
	quint64 best_slowest_time = m_forward ? 0xffffffff : 0;
	for (int channel = 0; channel < m_channel_number; ++channel)
	{

		if (mFinished[channel])
			continue;

		if (mMovie[channel].count()==0)
			continue;

		quint64 curr_channel_time = mMovie[channel].at(mCurrIndex[channel]).time;

		if ((m_forward && curr_channel_time <= best_slowest_time) || (!m_forward && curr_channel_time >= best_slowest_time))
		{
			best_slowest_time = curr_channel_time;
			slowest_channel = channel;
		}		
	}

	return slowest_channel;

}

void CLArchiveStreamReader::parse_channel_data(int channel, int data_version, char* data, unsigned int len)
{
	if (data_version!=1)
		return;

	char* endpoint = data + len - 1;

	unsigned long shift = 0;

	while(endpoint-data>=17)
	{
		ArchiveFrameInfo finfo;

		finfo.size = *((unsigned int*)data); data+=4;
		finfo.abs_time = *((quint64*)data); data+=8;
		finfo.codec = *((unsigned int*)data); data+=4;
		finfo.keyFrame = *((unsigned char*)data); data+=1;
		finfo.shift = shift;
		shift+= finfo.size;

		mMovie[channel].push_back(finfo);
	}
}

