#ifndef abstract_media_data_h_112
#define abstract_media_data_h_112

#include <QVector>
#include <QAudioFormat>
#include "libavcodec/avcodec.h"
#include "datapacket.h"
#include "utils/common/bytearray.h"

struct AVCodecContext;

class QnMediaContext
{
public:
    QnMediaContext(AVCodecContext* ctx);
    QnMediaContext(CodecID codecId);
    QnMediaContext(const quint8* payload, int dataSize);
    ~QnMediaContext();
    AVCodecContext* ctx() const;
    bool equalTo(QnMediaContext* other) const;
private:
    AVCodecContext* m_ctx;
};
typedef QSharedPointer<QnMediaContext> QnMediaContextPtr;

struct QnAbstractMediaData : public QnAbstractDataPacket
{
    enum MediaFlags {MediaFlags_None = 0, 
                     //MediaFlags_Key = 1,
                     MediaFlags_AfterEOF = 2,
                     MediaFlags_BOF = 4,
                     MediaFlags_LIVE = 8,
                     MediaFlags_SingleShot = 16,
                    };

	QnAbstractMediaData(unsigned int alignment, unsigned int capacity)
        : data(alignment, capacity),
        flags(MediaFlags_None),
        channelNumber(0),
        subChannelNumber(0),
        context(0),
        opaque(0)
	{
	}

	virtual ~QnAbstractMediaData()
	{
	}

	enum DataType {VIDEO, AUDIO, CONTAINER, META_V1};

	CLByteArray data;
	DataType dataType;
	CodecID compressionType;
    //quint8 containerFormat[5]; // used for container dataType only instead compressionType;
	qint64 timestamp; // mksec // 10^-6
    unsigned flags;
    quint32 channelNumber;     // video or audio channel number; some devices might have more that one sensor.
    quint32 subChannelNumber; // video camera can provide combination of different context at single channel (H.264 hi-res and low-res for example)
    //void* context;
    QnMediaContextPtr context;
    int opaque;
private:
	QnAbstractMediaData() : 
       data(0,1){};
};
typedef QSharedPointer<QnAbstractMediaData> QnAbstractMediaDataPtr;

struct QnEmptyMediaData : public QnAbstractMediaData
{
    QnEmptyMediaData(): QnAbstractMediaData(16,0)
    {
    }
};


struct QnCompressedVideoData : public QnAbstractMediaData
{
	QnCompressedVideoData(unsigned int alignment, unsigned int capacity, QnMediaContextPtr ctx = QnMediaContextPtr(0))
        : QnAbstractMediaData(alignment, qMin(capacity, (unsigned int)10 * 1024 * 1024))
	{
		dataType = VIDEO;
		//useTwice = false;
		context = ctx;
		ignore = false;
        flags = 0;
        width = height = -1;
	}

	int width;
	int height;
	//bool keyFrame;
    //int flags;
	bool ignore;
};

typedef QSharedPointer<QnCompressedVideoData> QnCompressedVideoDataPtr;


struct QnMetaDataV1 : public QnAbstractMediaData
{
    enum {MD_WIDTH = 42, MD_HIGHT = 32};
    QnMetaDataV1()
        : QnAbstractMediaData(0, MD_WIDTH*MD_HIGHT/8 + 1)
    {
        dataType = META_V1;
        //useTwice = false;

        flags = 0;
        i_mask = 0x01;
        
    }

    // ti check if we've got motion at 
    bool isMotionAt(int x, int y) const
    {
        Q_ASSERT(x<MD_WIDTH);
        Q_ASSERT(y<MD_HIGHT);

        int shift = y*x;
        unsigned char b = *((unsigned char*)data.data() + shift/8 );
        return (b >> (shift&7) ) & 0x01;
    }

    bool isInput(int index) const
    {
        unsigned char b = *((unsigned char*)data.data() + MD_WIDTH * MD_HIGHT/8);
        return (b>>index) & 1 ;
    }

    unsigned char i_mask;

};

typedef QSharedPointer<QnMetaDataV1> QnMetaDataV1Ptr;


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
	QnCompressedAudioData (unsigned int alignment, unsigned int capacity, QnMediaContextPtr ctx = QnMediaContextPtr(0))
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
