#ifndef MEDIADATAPACKET_H
#define MEDIADATAPACKET_H

#include <QtMultimedia/QAudioFormat>

#include "utils/ffmpeg/ffmpeg_global.h"
#include "datapacket.h"
#include "common/bytearray.h"

struct AVCodecContext;

struct QnAbstractMediaDataPacket : public QnAbstractDataPacket
{
    enum DataType { VIDEO, AUDIO };

    enum MediaFlags { MediaFlags_None = 0, MediaFlags_AfterEOF = 1 };

    QnAbstractMediaDataPacket(unsigned int alignment, unsigned int capacity)
        : data(alignment, capacity),
        flags(MediaFlags_None),
        channelNumber(0),
        context(0)
    {}
    virtual ~QnAbstractMediaDataPacket()
    {}

    CLByteArray data;
    DataType dataType;
    int compressionType;
    qint64 timestamp; // mksec // 10^-6
    unsigned flags;
    quint32 channelNumber; // video channel number; some devices might have more that one sensor.
    void* context;

private:
    QnAbstractMediaDataPacket() : data(0, 1) {}
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
    bool ignore;
};

typedef QSharedPointer<QnCompressedVideoData> QnCompressedVideoDataPtr;


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

      void fromAvStream(AVCodecContext* c);

      QVector<quint8> extraData; // codec extra data
      int bitrate;
      int channel_layout;
      int block_align;
};


struct QnCompressedAudioData : public QnAbstractMediaDataPacket
{
    QnCompressedAudioData (unsigned int alignment, unsigned int capacity, void* ctx)
        : QnAbstractMediaDataPacket(alignment, capacity)
    {
        dataType = AUDIO;
        context = ctx;
    }

    CLCodecAudioFormat format;
    quint64 duration;
};

typedef QSharedPointer<QnCompressedAudioData> QnCompressedAudioDataPtr;

#endif // MEDIADATAPACKET_H
