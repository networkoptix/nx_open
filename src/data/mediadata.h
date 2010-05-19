#ifndef abstract_media_data_h_112
#define abstract_media_data_h_112

#include "data.h"
#include "../base/bytearray.h"
#include <QString>
#include <QTime>



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
	enum CompressionType {JPEG, H264, RGB24, YUV420, YUV422};

	CLByteArray data;
	DataType dataType;
	CompressionType compressionType;


	quint32 channel_num;
	QTime time;


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
	}

	int width;
	int height;
	bool keyFrame;

};

typedef CLThreadQueue<CLAbstractData*> CLDataQueue;

#endif //abstract_media_data_h_112