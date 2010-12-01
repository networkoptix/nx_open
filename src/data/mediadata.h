#ifndef abstract_media_data_h_112
#define abstract_media_data_h_112

#include "data.h"
#include "../base/bytearray.h"
#include <QString>
#include <QTime>
#include "decoders/video/frame_info.h"



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
	CLVideoCodecType compressionType;


	quint32 channel_num;
	//QTime time;


	quint32 source_id;

private:

	CLAbstractMediaData():data(0,1){};

};


struct CLCompressedVideoData : public CLAbstractMediaData
{
	CLCompressedVideoData(unsigned int alignment, unsigned int capacity):
	CLAbstractMediaData(alignment, capacity)
	
	{
		dataType = VIDEO;
		use_twice = false;
	}

	int width;
	int height;
	bool keyFrame;
	bool use_twice; // some decoders delay video frame by one;

};

typedef CLThreadQueue<CLAbstractData*> CLDataQueue;

#endif //abstract_media_data_h_112