#ifndef abstract_media_data_h_112
#define abstract_media_data_h_112

#include "data.h"
#include "../base/bytearray.h"


enum CLCodecType 
{
	CL_JPEG, 
	CL_H264,
	CL_WMV3,
	CL_MPEG2, 
	CL_MPEG4, 
	CL_MSVIDEO1, 
	CL_MSMPEG4V2,
	CL_MSMPEG4V3,
	CL_MPEG1VIDEO,

	CL_MP2,
	CL_MP3, 
	CL_AC3,
	CL_AAC, 
	CL_WMAV2, 
	CL_WMAPRO,
	CL_ADPCM_MS,
	CL_UNKNOWN, 
	CL_VARIOUSE_DECODERS};



struct CLAbstractMediaData : public CLAbstractData
{

	CLAbstractMediaData(unsigned int alignment, unsigned int capacity):
	data(alignment, capacity)
	{
		
	};
	virtual ~CLAbstractMediaData()
	{

	}

	enum DataType {VIDEO, AUDIO};
	

	CLByteArray data;
	DataType dataType;
	CLCodecType compressionType;
	quint64 timestamp; // mksec // 10^-6

private:

	CLAbstractMediaData():data(0,1){};

};


struct CLCompressedVideoData : public CLAbstractMediaData
{
	CLCompressedVideoData(unsigned int alignment, unsigned int capacity, void* ctx = 0):
		CLAbstractMediaData(alignment, qMin(capacity, (unsigned int)10*1024*1024))
	{
		dataType = VIDEO;
		use_twice = false;
		context = ctx;
	}

	int width;
	int height;
	bool keyFrame;
	bool use_twice; // some decoders delay video frame by one;
	quint32 channel_num; // video channel number; some devices might have more that one sensor.

	void* context;
};


struct CLCompressedAudioData : public CLAbstractMediaData
{
	CLCompressedAudioData (unsigned int alignment, unsigned int capacity, void* ctx):
	CLAbstractMediaData(alignment, capacity)

	{
		dataType = AUDIO;
		context = ctx;
	}

	int freq;
	int channels;

	void* context;
};


typedef CLThreadQueue<CLAbstractData*> CLDataQueue;

#endif //abstract_media_data_h_112