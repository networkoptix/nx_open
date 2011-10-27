#ifndef abstract_media_data_h_112
#define abstract_media_data_h_112

#include <QVector>
#include <QAudioFormat>
#include "libavcodec/avcodec.h"
#include "datapacket.h"
#include "utils/common/bytearray.h"

struct QnAbstractMediaData : public QnAbstractDataPacket
{
    enum MediaFlags {MediaFlags_None = 0, 
                     //MediaFlags_Key = 1,
                     MediaFlags_AfterEOF = 2,
                     MediaFlags_BOF = 4
                    };

	QnAbstractMediaData(unsigned int alignment, unsigned int capacity)
        : data(alignment, capacity),
        flags(MediaFlags_None),
        channelNumber(0),
        subChannelNumber(0),
        context(0)
	{
	}

	virtual ~QnAbstractMediaData()
	{
	}

	enum DataType {VIDEO, AUDIO, CONTAINER};

	CLByteArray data;
	DataType dataType;
	CodecID compressionType;
    //quint8 containerFormat[5]; // used for container dataType only instead compressionType;
	qint64 timestamp; // mksec // 10^-6
    unsigned flags;
    quint32 channelNumber;     // video or audio channel number; some devices might have more that one sensor.
    quint32 subChannelNumber; // video camera can provide combination of different context at single channel (H.264 hi-res and low-res for example)
    void* context;
private:
	QnAbstractMediaData() : 
       data(0,1){};
};
typedef QSharedPointer<QnAbstractMediaData> QnAbstractMediaDataPtr;

struct QnCompressedVideoData : public QnAbstractMediaData
{
	QnCompressedVideoData(unsigned int alignment, unsigned int capacity, void* ctx = 0)
        : QnAbstractMediaData(alignment, qMin(capacity, (unsigned int)10 * 1024 * 1024))
	{
		dataType = VIDEO;
		useTwice = false;
		context = ctx;
		ignore = false;
        flags = 0;
        width = height = -1;
	}

	int width;
	int height;
	//bool keyFrame;
    //int flags;
	bool useTwice; // some decoders delay video frame by one;
	bool ignore;
};

typedef QSharedPointer<QnCompressedVideoData> QnCompressedVideoDataPtr;

class QnCodecAudioFormat: public QAudioFormat
{
public:
    QnCodecAudioFormat():
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


struct QnCompressedAudioData : public QnAbstractMediaData
{
	QnCompressedAudioData (unsigned int alignment, unsigned int capacity, void* ctx = 0)
        : QnAbstractMediaData(alignment, capacity)
	{
		dataType = AUDIO;
        duration = 0;
        context = ctx;
	}
    QnCodecAudioFormat format;
    quint64 duration;
};
typedef QSharedPointer<QnCompressedAudioData> QnCompressedAudioDataPtr;

typedef CLThreadQueue<QnAbstractDataPacketPtr> CLDataQueue;

#endif //abstract_media_data_h_112
