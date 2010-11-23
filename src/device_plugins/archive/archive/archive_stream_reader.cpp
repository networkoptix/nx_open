#include "archive_stream_reader.h"
#include "device\device.h"
#include "base\bytearray.h"
#include "data\mediadata.h"
#include <QDateTime>

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
	mAdaptiveSleep.afterdelay();
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
			min_date_time = QDateTime::fromMSecsSinceEpoch(mMovie[channel].at(0).abs_time);
			first = false;
		}

		QDateTime time = QDateTime::fromMSecsSinceEpoch(mMovie[channel].at(0).abs_time);

		if (time<min_date_time)
			min_date_time = time;
	}
	
	//===========time_ms===================
	
	for (int channel = 0; channel < m_channel_number; ++channel)
	{
		for(int i = 0; i < mMovie[channel].count();++i)
		{
			ArchiveFrameInfo& info = mMovie[channel][i];

			QDateTime time = QDateTime::fromMSecsSinceEpoch(info.abs_time);

			unsigned long time_ms = min_date_time.msecsTo(time);
			info.time_ms = time_ms;

			if (time_ms>m_len_msec)
				m_len_msec = time_ms;
		}
	}


}


unsigned long CLArchiveStreamReader::currTime() const
{
	QMutexLocker mutex(&m_cs);
	int slowe_channel = slowest_channel();
	return mMovie[slowe_channel].at(mCurrIndex[slowe_channel]).time_ms;
}


CLAbstractMediaData* CLArchiveStreamReader::getNextData()
{
	if (m_firsttime)
	{
		//init_data();
		m_firsttime = false;
	}

	if (!isSingleShotMode())
		mAdaptiveSleep.sleep(m_need_tosleep);


	// will return next channel ( if channel is finished it will not be selected
	int channel;

	{
		QMutexLocker mutex(&m_cs);
		channel = slowest_channel(); 
	}


	if (channel<0)
	{
		// shoud not get here;
		// if we here => some data file corrupted
		msleep(20); // to avoid CPU load
		return 0;
	}


	QMutexLocker mutex(&m_cs);

	const ArchiveFrameInfo &finfo = mMovie[channel].at(mCurrIndex[channel]);

	CLCompressedVideoData* videoData = new CLCompressedVideoData(8,finfo.size);
	CLByteArray& data = videoData->data;

	data.prepareToWrite(finfo.size);
	int readed = m_data_stream[channel].readRawData(data.data(), finfo.size);
	data.done(readed);

	videoData->compressionType = CLAbstractMediaData::CompressionType(finfo.codec);
	videoData->keyFrame = finfo.keyFrame;
	videoData->channel_num = channel;


	if (videoData->keyFrame)
		m_gotKeyFrame[channel] = true;

	//=================

	m_need_tosleep = 0;

	// changing current index of this channel
	int new_index = nextFrameIndex(false, channel, mCurrIndex[channel], needKeyData(channel), mForward);
	if (new_index<0)
	{
		mFinished[channel]=true; // slowest_channel will not return this channel any more 
		if (reachedTheEnd())
		{
			if (mForward)
				jumpTo(0, false);
			else
				jumpTo(len_msec(), false);
		}
	}
	else
	{
		if (new_index - mCurrIndex[channel] !=1 )
		{
			int shift = mMovie[channel].at(new_index).shift;
			m_data_file[channel].seek(shift);
		}

		unsigned long this_time = mMovie[channel].at(mCurrIndex[channel]).time_ms;
		mCurrIndex[channel] = new_index;

		int next_channel = slowest_channel();
		unsigned long next_time = mMovie[channel].at(mCurrIndex[next_channel]).time_ms;

		m_need_tosleep = labs(next_time - this_time);

	}


	//=================
	if (isSingleShotMode())
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


void CLArchiveStreamReader::channeljumpTo(unsigned long msec, int channel)
{
	
	int new_index = findBestIndex(channel, msec);
	if (new_index<0)
		return;

	mCurrIndex[channel] = nextFrameIndex(true, channel, new_index, true, mForward);

	if (mCurrIndex[channel]==-1)
		mCurrIndex[channel] = nextFrameIndex(true, channel, new_index, true, !mForward);

	mFinished[channel] = (mCurrIndex[channel]==-1);


	if (mCurrIndex[channel]==-1)
		return;

	unsigned long shift = mMovie[channel].at(mCurrIndex[channel]).shift;
	m_data_file[channel].seek(shift);
}

int CLArchiveStreamReader::findBestIndex(int channel, unsigned long msec) const
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


	while(index2 - index1 > 1)
	{
		int new_index = (index1+index2)/2;
		unsigned long new_index_time = mMovie[channel].at(new_index).time_ms;

		if (new_index_time>=msec)
			index2 = new_index;
		else
			index1 = new_index;
	}


	return mForward ? index1 : index2;
}

int CLArchiveStreamReader::slowest_channel() const
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

