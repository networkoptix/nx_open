/**********************************************************
* 23 jun 2014
* akolesnikov
***********************************************************/

#ifndef VIDEO_DATA_PACKET_H
#define VIDEO_DATA_PACKET_H

#ifdef ENABLE_DATA_PROVIDERS

#include "media_data_packet.h"


//!Contains video data specific fields. Video data buffer stored in a child class
class QnCompressedVideoData
:
    public QnAbstractMediaData
{
public:
    int width;
    int height;
    //bool keyFrame;
    //int flags;
    //bool ignore;
    QnMetaDataV1Ptr motion;
    qint64 pts;

    QnCompressedVideoData( QnMediaContextPtr ctx = QnMediaContextPtr(0) );

    //!Implementation of QnAbstractMediaData::clone
    /*!
        Does nothing. Overridden method MUST return \a QnWritableCompressedVideoData
    */
    virtual QnCompressedVideoData* clone( QnAbstractAllocator* allocator = QnSystemAllocator::instance() ) const override = 0;

    void assign(const QnCompressedVideoData* other);
};

typedef QSharedPointer<QnCompressedVideoData> QnCompressedVideoDataPtr;
typedef QSharedPointer<const QnCompressedVideoData> QnConstCompressedVideoDataPtr;


//!Stores video data buffer using \a QnByteArray
class QnWritableCompressedVideoData
:
    public QnCompressedVideoData
{
public:
    QnByteArray m_data;

    QnWritableCompressedVideoData(
        unsigned int alignment = CL_MEDIA_ALIGNMENT,
        unsigned int capacity = 0,
        QnMediaContextPtr ctx = QnMediaContextPtr(0) );
    QnWritableCompressedVideoData(
        QnAbstractAllocator* allocator,
        unsigned int alignment = CL_MEDIA_ALIGNMENT,
        unsigned int capacity = 0,
        QnMediaContextPtr ctx = QnMediaContextPtr(0) );

    //!Implementation of QnAbstractMediaData::clone
    virtual QnWritableCompressedVideoData* clone( QnAbstractAllocator* allocator = QnSystemAllocator::instance() ) const override;
    //!Implementation of QnAbstractMediaData::data
    virtual const char* data() const override;
    //!Implementation of QnAbstractMediaData::dataSize
    virtual size_t dataSize() const override;

private:
    void assign(const QnWritableCompressedVideoData* other);
};

typedef QSharedPointer<QnWritableCompressedVideoData> QnWritableCompressedVideoDataPtr;
typedef QSharedPointer<const QnWritableCompressedVideoData> QnConstWritableCompressedVideoDataPtr;

#endif // ENABLE_DATA_PROVIDERS

#endif  //VIDEO_DATA_PACKET_H
