#ifndef abstract_media_data_h_112
#define abstract_media_data_h_112

#include <QVector>
#include <QAudioFormat>
#include "data.h"
#include "base/ffmpeg_codec.h"
#include "../base/bytearray.h"
//#include "decoders/audio/ffmpeg_audio.h"
#include "libavcodec/avcodec.h"

struct CLAbstractMediaData : public CLAbstractData
{
    enum MediaFlags {MediaFlags_None = 0, MediaFlags_AfterEOF = 2};

	CLAbstractMediaData(unsigned int alignment, unsigned int capacity)
        : data(alignment, capacity),
        flags(MediaFlags_None)
	{
	}

	virtual ~CLAbstractMediaData()
	{
	}

	enum DataType {VIDEO, AUDIO};

	CLByteArray data;
	DataType dataType;
	CodecID compressionType;
	qint64 timestamp; // mksec // 10^-6
    unsigned flags;

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
        flags = 0;
	}

	int width;
	int height;
	//bool keyFrame;
    //int flags;
	bool useTwice; // some decoders delay video frame by one;
	quint32 channelNumber; // video channel number; some devices might have more that one sensor.
	bool ignore;
	void* context;
};

class CLCodecAudioFormat: public QAudioFormat
{
public:
    CLCodecAudioFormat():
            QAudioFormat(),
            bitrate(0),
            channel_layout(0),
            block_align(0)
    {
    }

    void fromAvStream(AVCodecContext* c)
    {
        if (c->sample_rate)
            setFrequency(c->sample_rate);

        if (c->channels) 
            setChannels(c->channels);

        //setCodec("audio/pcm");
        setByteOrder(QAudioFormat::LittleEndian);

        switch(c->sample_fmt)
        {
        case SAMPLE_FMT_U8: ///< unsigned 8 bits
            setSampleSize(8);
            setSampleType(QAudioFormat::UnSignedInt);
            break;

        case SAMPLE_FMT_S16: ///< signed 16 bits
            setSampleSize(16);
            setSampleType(QAudioFormat::SignedInt);
            break;

        case SAMPLE_FMT_S32:///< signed 32 bits
            setSampleSize(32);
            setSampleType(QAudioFormat::SignedInt);
            break;

        case AV_SAMPLE_FMT_FLT:
            setSampleSize(32);
            setSampleType(QAudioFormat::Float);
            break;

        default:
            break;
        }

        if (c->extradata_size > 0)
        {
            extraData.resize(c->extradata_size);
            memcpy(&extraData[0], c->extradata, c->extradata_size);
        }
        bitrate = c->bit_rate;
        channel_layout = c->channel_layout;
        block_align = c->block_align;
    }

    QVector<quint8> extraData; // codec extra data
    int bitrate;
    int channel_layout;
    int block_align;
};


struct CLCompressedAudioData : public CLAbstractMediaData
{
	CLCompressedAudioData (unsigned int alignment, unsigned int capacity)
        : CLAbstractMediaData(alignment, capacity)
	{
		dataType = AUDIO;
	}
    CLCodecAudioFormat format;
    quint64 duration;
};

typedef CLThreadQueue<CLAbstractData*> CLDataQueue;

#endif //abstract_media_data_h_112
