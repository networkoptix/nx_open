/**********************************************************
* 23 jun 2014
* akolesnikov
***********************************************************/

#ifndef VIDEO_DATA_PACKET_H
#define VIDEO_DATA_PACKET_H

#ifdef ENABLE_DATA_PROVIDERS

#include <memory>

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

    QnCompressedVideoData( QnConstMediaContextPtr ctx = QnConstMediaContextPtr(nullptr) );

    //!Implementation of QnAbstractMediaData::clone
    /*!
        Does nothing. Overridden method MUST return \a QnWritableCompressedVideoData
    */
    virtual QnCompressedVideoData* clone( QnAbstractAllocator* allocator = QnSystemAllocator::instance() ) const override = 0;

    void assign(const QnCompressedVideoData* other);
};

typedef std::shared_ptr<QnCompressedVideoData> QnCompressedVideoDataPtr;
typedef std::shared_ptr<const QnCompressedVideoData> QnConstCompressedVideoDataPtr;


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
        QnConstMediaContextPtr ctx = QnConstMediaContextPtr(nullptr) );
    QnWritableCompressedVideoData(
        QnAbstractAllocator* allocator,
        unsigned int alignment = CL_MEDIA_ALIGNMENT,
        unsigned int capacity = 0,
        QnConstMediaContextPtr ctx = QnConstMediaContextPtr(nullptr) );

    //!Implementation of QnAbstractMediaData::clone
    virtual QnWritableCompressedVideoData* clone( QnAbstractAllocator* allocator = QnSystemAllocator::instance() ) const override;
    //!Implementation of QnAbstractMediaData::data
    virtual const char* data() const override;
    //!Implementation of QnAbstractMediaData::dataSize
    virtual size_t dataSize() const override;

private:
    void assign(const QnWritableCompressedVideoData* other);
};

typedef std::shared_ptr<QnWritableCompressedVideoData> QnWritableCompressedVideoDataPtr;
typedef std::shared_ptr<const QnWritableCompressedVideoData> QnConstWritableCompressedVideoDataPtr;

#endif // ENABLE_DATA_PROVIDERS

#endif  //VIDEO_DATA_PACKET_H
