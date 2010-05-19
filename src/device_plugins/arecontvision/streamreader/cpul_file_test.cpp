#include "cpul_file_test.h"
#include <QTextStream>
#include "../../../base/log.h"
#include "../../../data/mediadata.h"



//=========================================================

AVTestFileStreamreader::AVTestFileStreamreader (CLDevice* dev):
CLClientPullStreamreader(dev)
{
	const int max_data_size = 10*1024*1024;
	data = new unsigned char [max_data_size];
	descr = new unsigned char [max_data_size];
	pdata = data;
	pdescr = (int*)descr;


	FILE *fdata = fopen("test.264", "rb");
	FILE *fdescr = fopen("test.264.desc", "rb");

	data_len = fread(data,1,max_data_size,fdata);
	descr_data_len = fread(descr,1,max_data_size,fdescr);

	fclose(fdata);
	fclose(fdescr);

}

AVTestFileStreamreader::~AVTestFileStreamreader()
{
	delete[] data;
	delete[] descr;
}

CLAbstractMediaData* AVTestFileStreamreader::getNextData()
{
	CLCompressedVideoData* videoData = new CLCompressedVideoData(8,400*1024);
	CLByteArray& img = videoData->data;

	int descr_len = *pdescr; pdescr++;
	if ((unsigned char*)pdescr - descr >= descr_data_len)
	{
		// from very beginin
		pdata = data;
		pdescr = (int*)descr;
		descr_len = *pdescr; pdescr++;
	}

	//len*=2;



	img.write((char*)pdata, descr_len);
	pdata+=descr_len;


	
	
	videoData->compressionType = CLAbstractMediaData::H264;
	videoData->width = 1600;
	videoData->height = 1184;
	videoData->channel_num = 0;

	
	return videoData;

}

