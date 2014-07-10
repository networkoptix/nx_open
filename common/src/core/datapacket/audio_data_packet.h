/**********************************************************
* 23 jun 2014
* akolesnikov
***********************************************************/

#ifndef AUDIO_DATA_PACKET_H
#define AUDIO_DATA_PACKET_H

#ifdef ENABLE_DATA_PROVIDERS

#include "media_data_packet.h"


class QnCodecAudioFormat
:
    public QnAudioFormat
{
public:
    QnCodecAudioFormat();
    QnCodecAudioFormat(QnMediaContextPtr ctx);

    QnCodecAudioFormat& fromAvStream(QnMediaContextPtr ctx);
    void fromAvStream(AVCodecContext* c);

    QVector<quint8> extraData; // codec extra data
    int bitrate;
    int channel_layout;
    int block_align;
    int m_bitsPerSample;

private:
    int m_frequency;
};


class QnCompressedAudioData
:
    public QnAbstractMediaData
{
public:
    //QnCodecAudioFormat format;
    quint64 duration;

    QnCompressedAudioData( QnMediaContextPtr ctx );

    void assign(const QnCompressedAudioData* other);
};

typedef QSharedPointer<QnCompressedAudioData> QnCompressedAudioDataPtr;
typedef QSharedPointer<const QnCompressedAudioData> QnConstCompressedAudioDataPtr;


//!Stores video data buffer using \a QnByteArray
class QnWritableCompressedAudioData
:
    public QnCompressedAudioData
{
public:
    QnByteArray m_data;

    QnWritableCompressedAudioData(
        unsigned int alignment = CL_MEDIA_ALIGNMENT,
        unsigned int capacity = 0,
        QnMediaContextPtr ctx = QnMediaContextPtr(0) );
    QnWritableCompressedAudioData(
        QnAbstractAllocator* allocator,
        unsigned int alignment = CL_MEDIA_ALIGNMENT,
        unsigned int capacity = 0,
        QnMediaContextPtr ctx = QnMediaContextPtr(0) );

    //!Implementation of QnAbstractMediaData::clone
    virtual QnWritableCompressedAudioData* clone( QnAbstractAllocator* allocator = QnSystemAllocator::instance() ) const override;
    //!Implementation of QnAbstractMediaData::data
    virtual const char* data() const override;
    //!Implementation of QnAbstractMediaData::dataSize
    virtual size_t dataSize() const override;

private:
    void assign(const QnWritableCompressedAudioData* other);
};

typedef QSharedPointer<QnWritableCompressedAudioData> QnWritableCompressedAudioDataPtr;
typedef QSharedPointer<const QnWritableCompressedAudioData> QnConstWritableCompressedAudioDataPtr;

#endif // ENABLE_DATA_PROVIDERS

#endif  //AUDIO_DATA_PACKET_H
