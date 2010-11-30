#include "fake_file_streamreader.h"
#include <QTextStream>
#include "../../../base/log.h"
#include "../../../data/mediadata.h"
#include "device/device.h"



unsigned char* FakeStreamReader::data = 0 ;
unsigned char* FakeStreamReader::descr = 0;
int FakeStreamReader::data_len = 0;
int FakeStreamReader::descr_data_len = 0;

//=========================================================

FakeStreamReader::FakeStreamReader (CLDevice* dev, int channels):
CLClientPullStreamreader(dev),
m_channels(channels),
m_curr_channel(0),
m_sleep(20)
{

	dev->addDeviceTypeFlag(CLDevice::ARCHIVE);

	const int max_data_size = 200*1024*1024;

	if (data == 0) // very first time
	{
		data = new unsigned char [max_data_size];
		descr = new unsigned char [max_data_size/10];

		data_len = 0;
		descr_data_len = 0;

		FILE *fdata = fopen("c:/photo/test.264", "rb");
		FILE *fdescr = fopen("c:/photo/test.264.desc", "rb");

		if (fdata && fdescr)
		{
			data_len = fread(data,1,max_data_size,fdata);
			descr_data_len = fread(descr,1,max_data_size/10,fdescr);

			fclose(fdata);
			fclose(fdescr);
		}
		else
		{
			if (fdata)
				fclose(fdata);

			if (fdescr)
				fclose(fdescr);
		}



	}


	for (int i = 0; i < m_channels; ++i)
	{
		pdata[i] = data;
		pdescr[i] = (int*)descr;
	}

}

FakeStreamReader::~FakeStreamReader()
{
	//delete[] data;
	//delete[] descr;
}

CLAbstractMediaData* FakeStreamReader::getNextData()
{

	//m_sleep.sleep(1000/8);// 33 fps
	qreal fps = 30;
	//qreal fps = 6.5;
	CLSleep::msleep(1000/fps);
	//CLSleep::msleep(1000/1);

	if (data_len==0 || descr_data_len == 0)
		return 0;

	
	CLCompressedVideoData* videoData = new CLCompressedVideoData(8,400*1024);
	CLByteArray& img = videoData->data;

	int descr_len = *(pdescr[m_curr_channel]); (pdescr[m_curr_channel])++;
	if ((unsigned char*)(pdescr[m_curr_channel]) - descr >= descr_data_len)
	{
		// from very beginin
		pdata[m_curr_channel] = data;
		pdescr[m_curr_channel] = (int*)descr;
		descr_len = *(pdescr[m_curr_channel]); (pdescr[m_curr_channel])++;
	}

	//len*=2;



	img.write((char*)(pdata[m_curr_channel]), descr_len);
	pdata[m_curr_channel]+=descr_len;


	
	
	videoData->compressionType = CL_H264;
	videoData->width = 1600;
	videoData->height = 1184;
	videoData->channel_num = m_curr_channel;

	m_curr_channel++;

	if (m_curr_channel==m_channels)
		m_curr_channel=0;

	

	return videoData;

}

