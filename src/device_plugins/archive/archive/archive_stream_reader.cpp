#include "archive_stream_reader.h"
#include "device\device.h"
#include "base\bytearray.h"
#include "data\mediadata.h"

CLArchiveStreamReader::CLArchiveStreamReader(CLDevice* dev):
CLAbstractArchiveReader(dev),
m_firsttime(true)

{
}

CLArchiveStreamReader::~CLArchiveStreamReader()
{

}

void CLArchiveStreamReader::init_data()
{
	for (int channel = 0; channel < m_channel_number; ++channel)
	{
		QString fn;
		QTextStream stream(&fn);
		stream << m_device->getUniqueId() << "/channel_";
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

	

}

void CLArchiveStreamReader::jumpTo(unsigned long msec, int channel)
{
	CLAbstractArchiveReader::jumpTo(msec,channel);
	unsigned long shift = mMovie[channel].at(mCurrIndex[channel]).shift;

	m_data_file[channel].seek(shift);
}

CLAbstractMediaData* CLArchiveStreamReader::getNextData()
{
	if (m_firsttime)
	{
		init_data();
		m_firsttime = false;
	}

	if (!isSingleShotMode())
		mAdaptiveSleep.sleep(m_need_tosleep);



	int channel = slowest_channel(); 

	if (channel<0)
	{
		// shoud not get here;
		// if we here => some data file corrupted
		msleep(20); // to avoid CPU load
		return 0;
	}
	
	const ArchiveFrameInfo &finfo = mMovie[channel].at(mCurrIndex[channel]);

	CLCompressedVideoData* videoData = new CLCompressedVideoData(8,finfo.size);
	CLByteArray& data = videoData->data;

	data.prepareToWrite(finfo.size);
	int readed = m_data_stream[channel].readRawData(data.data(), finfo.size);
	data.done(readed);

	videoData->compressionType = CLAbstractMediaData::CompressionType(finfo.codec);
	videoData->keyFrame = finfo.keyFrame;
	videoData->channel_num = channel;

	//=================
	
	m_need_tosleep = 0;

	if (reachedTheEnd())
	{
		if (mForward)
			jumpTo(0, false);
		else
			jumpTo(len_msec(), false);
	}
	else
	{
		// changing current index of this channel
		int new_index = nextFrameIndex(false, channel, mCurrIndex[channel], needKeyData(channel), mForward);
		if (new_index<0)
			mFinished[channel]=true; // slowest_channel will not return this channel any more 
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
	}

	//=================
	if (isSingleShotMode())
		pause();

	return videoData;

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