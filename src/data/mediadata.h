#ifndef abstract_media_data_h_112
#define abstract_media_data_h_112

#include "data.h"
#include "base/ffmpeg_codec.h"
#include "../base/bytearray.h"

struct CLAbstractMediaData : public CLAbstractData
{
	CLAbstractMediaData(unsigned int alignment, unsigned int capacity)
        : data(alignment, capacity)
	{
	}

	virtual ~CLAbstractMediaData()
	{
	}

	enum DataType {VIDEO, AUDIO};

	CLByteArray data;
	DataType dataType;
	CodecID compressionType;
	quint64 timestamp; // mksec // 10^-6

private:
	CLAbstractMediaData() : 
       data(0,1){};
};

struct CLCompressedVideoData : public CLAbstractMediaData
{
	CLCompressedVideoData(unsigned int alignment, unsigned int capacity, void* ctx = 0)
        : CLAbstractMediaData(alignment, qMin(capacity, (unsigned int)10 * 1024 * 1024))
	{
		dataType = VIDEO;
		useTwice = false;
		context = ctx;
		ignore = false;
	}

	int width;
	int height;
	bool keyFrame;
	bool useTwice; // some decoders delay video frame by one;
	quint32 channelNumber; // video channel number; some devices might have more that one sensor.
	bool ignore;
	void* context;
};

struct CLCompressedAudioData : public CLAbstractMediaData
{
	CLCompressedAudioData (unsigned int alignment, unsigned int capacity, void* ctx)
        : CLAbstractMediaData(alignment, capacity)
	{
		dataType = AUDIO;
		context = ctx;
	}

	int freq;
	int channels;
    quint64 duration;
	void* context;
};

typedef CLThreadQueue<CLAbstractData*> CLDataQueue;

#endif //abstract_media_data_h_112
