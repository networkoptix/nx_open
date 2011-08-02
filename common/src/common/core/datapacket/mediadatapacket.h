#ifndef abstract_media_data_h_112
#define abstract_media_data_h_112

#include "datapacket.h"
#include "common/bytearray.h"


struct QnAbstractMediaDataPacket : public QnAbstractDataPacket
{
	QnAbstractMediaDataPacket(unsigned int alignment, unsigned int capacity)
        : data(alignment, capacity)
	{
	}

	virtual ~QnAbstractMediaDataPacket()
	{
	}

	enum DataType {VIDEO, AUDIO};

	CLByteArray data;
	DataType dataType;
	CodecID compressionType;
	quint64 timestamp; // mksec // 10^-6

private:
	QnAbstractMediaDataPacket() : 
       data(0,1){};
};

typedef QSharedPointer<QnAbstractMediaDataPacket> QnAbstractMediaDataPacketPtr;


struct QnCompressedVideoData : public QnAbstractMediaDataPacket
{
	QnCompressedVideoData(unsigned int alignment, unsigned int capacity, void* ctx = 0)
        : QnAbstractMediaDataPacket(alignment, qMin(capacity, (unsigned int)10 * 1024 * 1024))
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

typedef QSharedPointer<QnCompressedVideoData> QnCompressedVideoDataPtr;

struct QnCompressedAudioData : public QnAbstractMediaDataPacket
{
	QnCompressedAudioData (unsigned int alignment, unsigned int capacity, void* ctx)
        : QnAbstractMediaDataPacket(alignment, capacity)
	{
		dataType = AUDIO;
		context = ctx;
	}

	int freq;
	int channels;
    quint64 duration;
	void* context;
};

typedef QSharedPointer<QnCompressedAudioData> QnCompressedAudioDataPtr;

#endif //abstract_media_data_h_112
