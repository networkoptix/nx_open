#ifndef abstract_media_data_h_112
#define abstract_media_data_h_112

#include <QVector>
#include <QRect>
#include "libavcodec/avcodec.h"
#include "datapacket.h"
#include "utils/common/bytearray.h"
#include "utils/media/sse_helper.h"

#ifndef Q_OS_WIN
#include "utils/media/audioformat.h"
#else
#include <QAudioFormat>
#define QnAudioFormat QAudioFormat
#endif

struct AVCodecContext;

enum MediaQuality { MEDIA_Quality_High, MEDIA_Quality_Low, MEDIA_Quality_None};

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
                     MediaFlags_Ignore = 16,
                     
                     MediaFlags_ReverseReordered = 32,
                     MediaFlags_ReverseBlockStart = 64,
                     MediaFlags_Reverse = 128,

                     MediaFlags_LowQuality = 256,
                     MediaFlags_StillImage = 512
                    };

	QnAbstractMediaData(unsigned int alignment, unsigned int capacity)
        : data(alignment, capacity),
        flags(MediaFlags_None),
        channelNumber(0),
        subChannelNumber(0),
        context(0),
        opaque(0),
        compressionType(CODEC_ID_NONE)
	{
	}

	virtual ~QnAbstractMediaData()
	{
	}

	enum DataType {VIDEO, AUDIO, CONTAINER, META_V1, EMPTY_DATA};

	CLByteArray data;
	DataType dataType;
	CodecID compressionType;
    //quint8 containerFormat[5]; // used for container dataType only instead compressionType;
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
        dataType = EMPTY_DATA;
    }
};
typedef QSharedPointer<QnEmptyMediaData> QnEmptyMediaDataPtr;

struct QnMetaDataV1;
typedef QSharedPointer<QnMetaDataV1> QnMetaDataV1Ptr;


struct QnCompressedVideoData : public QnAbstractMediaData
{
	QnCompressedVideoData(unsigned int alignment, unsigned int capacity, QnMediaContextPtr ctx = QnMediaContextPtr(0))
        : QnAbstractMediaData(alignment, qMin(capacity, (unsigned int)10 * 1024 * 1024))
	{
		dataType = VIDEO;
		//useTwice = false;
		context = ctx;
		//ignore = false;
        flags = 0;
        width = height = -1;
	}


	int width;
	int height;
	//bool keyFrame;
    //int flags;
	//bool ignore;
    QnMetaDataV1Ptr motion;
    
};

typedef QSharedPointer<QnCompressedVideoData> QnCompressedVideoDataPtr;

enum {MD_WIDTH = 44, MD_HEIGHT = 32};
struct QnMetaDataV1 : public QnAbstractMediaData
{
    QnMetaDataV1(int initialValue = 0);

    /** 
    * Merge existing motion image with new motion image. Matrix is allowed col to col 
    * 0   1
    * |   |
    * \/  \/
    * |   | 
    * \/  \/
    * |   |
    * \/  \/
    */
    void addMotion(const quint8* data, qint64 timestamp);
    void addMotion(QnMetaDataV1Ptr data);

    // remove part of motion info by motion mask
    void removeMotion(const __m128i* data, int startIndex = 0, int endIndex = MD_WIDTH*MD_HEIGHT/128 - 1);

    // ti check if we've got motion at 
    static bool isMotionAt(int x, int y, char* mask);

    // ti check if we've got motion at 
    bool isMotionAt(int x, int y) const;

    void setMotionAt(int x, int y);

    bool mapMotion(const QRect& imageRect, const QRect& mRect);

    bool isInput(int index) const
    {
        //unsigned char b = *((unsigned char*)data.data() + MD_WIDTH * MD_HIGHT/8);
        //return (b>>index) & 1 ;
        return (m_input >> index) & 1;
    }

    bool containTime(const qint64 timeUsec) const;

    /** returns true if no motion detected */
    bool isEmpty() const;


    static void createMask(const QRegion& region,  char* mask, int* maskStart = 0, int* maskEnd = 0);

    unsigned char i_mask;
    quint8 m_input;
    qint64 m_duration;
private:
    qint64 m_firstTimestamp;

};


class QnCodecAudioFormat: public QnAudioFormat
{
public:
    QnCodecAudioFormat():
            QnAudioFormat(),
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
        setByteOrder(QnAudioFormat::LittleEndian);

        switch(c->sample_fmt)
        {
        case AV_SAMPLE_FMT_U8: ///< unsigned 8 bits
            setSampleSize(8);
            setSampleType(QnAudioFormat::UnSignedInt);
            break;

        case AV_SAMPLE_FMT_S16: ///< signed 16 bits
            setSampleSize(16);
            setSampleType(QnAudioFormat::SignedInt);
            break;

        case AV_SAMPLE_FMT_S32:///< signed 32 bits
            setSampleSize(32);
            setSampleType(QnAudioFormat::SignedInt);
            break;

        case AV_SAMPLE_FMT_FLT:
            setSampleSize(32);
            setSampleType(QnAudioFormat::Float);
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

//typedef CLThreadQueue<QnAbstractDataPacketPtr> CLDataQueue;

#endif //abstract_media_data_h_112
