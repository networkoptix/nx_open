// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <cstdint>
#include <optional>
#include <string>

#include <nx/media/config.h>
#include <nx/media/media_data_packet.h>
#include <nx/media/meta_data_packet.h>
#include <nx/utils/byte_array.h>
#include <nx/utils/shm_byte_array.h>

constexpr auto MAX_ALLOWED_FRAME_SIZE = 1024*1024*10;

namespace nx::media::ffmpeg {
class FfmpegSharedMemoryAllocator;
using FfmpegSharedMemoryAllocatorPtr = std::shared_ptr<FfmpegSharedMemoryAllocator>;
} // namespace nx::media::ffmpeg

//!Contains video data specific fields. Video data buffer stored in a child class
class NX_MEDIA_CORE_API QnCompressedVideoData: public QnAbstractMediaData
{
public:
    int width;
    int height;
    //bool keyFrame;
    //int flags;
    //bool ignore;
    FrameMetadata metadata;
    qint64 pts;

    QnCompressedVideoData( CodecParametersConstPtr ctx = CodecParametersConstPtr(nullptr) );

    //!Implementation of QnAbstractMediaData::clone
    /*!
        Does nothing. Overridden method MUST return \a QnWritableCompressedVideoData
    */
    virtual QnCompressedVideoData* clone() const override = 0;

    void assign(const QnCompressedVideoData* other);
};

typedef std::shared_ptr<QnCompressedVideoData> QnCompressedVideoDataPtr;
typedef std::shared_ptr<const QnCompressedVideoData> QnConstCompressedVideoDataPtr;


//!Stores video data buffer using \a nx::utils::ByteArray
class NX_MEDIA_CORE_API QnWritableCompressedVideoData: public QnCompressedVideoData
{
public:
    nx::utils::ByteArray m_data;

    /** Default FFMpeg alignment and padding are used. */
    QnWritableCompressedVideoData(size_t capacity = 0, CodecParametersConstPtr ctx = {});

    //!Implementation of QnAbstractMediaData::clone
    virtual QnWritableCompressedVideoData* clone() const override;
    //!Implementation of QnAbstractMediaData::data
    virtual const char* data() const override;
    //!Implementation of QnAbstractMediaData::dataSize
    virtual size_t dataSize() const override;

    virtual void setData(nx::utils::ByteArray&& buffer) override;

private:
    void assign(const QnWritableCompressedVideoData* other);
};

typedef std::shared_ptr<QnWritableCompressedVideoData> QnWritableCompressedVideoDataPtr;
typedef std::shared_ptr<const QnWritableCompressedVideoData> QnConstWritableCompressedVideoDataPtr;

class NX_MEDIA_CORE_API QnSharedMemoryCompressedVideoData: public QnCompressedVideoData
{
public:
    QnSharedMemoryCompressedVideoData(
        nx::utils::ShmByteArray data,
        CodecParametersConstPtr ctx = {});

    virtual QnWritableCompressedVideoData* clone() const override;
    virtual const char* data() const override;
    virtual size_t dataSize() const override;
    virtual void setData(nx::utils::ByteArray&& buffer) override;

    char* mutableData();
    const nx::utils::ShmByteArray& shmData() const;

private:
    nx::utils::ShmByteArray m_data;
};

using QnSharedMemoryCompressedVideoDataPtr = std::shared_ptr<QnSharedMemoryCompressedVideoData>;
using QnConstSharedMemoryCompressedVideoDataPtr =
    std::shared_ptr<const QnSharedMemoryCompressedVideoData>;

struct NX_MEDIA_CORE_API SharedMemoryCompressedVideoDataDescriptor
{
    const char* data = nullptr;
    size_t dataSize = 0;
    std::uintptr_t sharedMemoryHandle = 0;
    std::string sharedMemoryName;
};

/**
 * Allocate shared-memory-backed compressed frame storage of requested size.
 */
NX_MEDIA_CORE_API QnSharedMemoryCompressedVideoDataPtr createSharedMemoryCompressedVideoData(
    size_t dataSize,
    const nx::media::ffmpeg::FfmpegSharedMemoryAllocatorPtr& sharedMemoryAllocator,
    CodecParametersConstPtr ctx = {});

/**
 * Return true if compressed frame payload is backed by FfmpegSharedMemory.
 */
NX_MEDIA_CORE_API bool isCompressedFrameInSharedMemory(const QnCompressedVideoData* frame);

/**
 * Extract shared-memory descriptor from compressed frame.
 * Returns std::nullopt for non-SHM-backed frames.
 */
NX_MEDIA_CORE_API std::optional<SharedMemoryCompressedVideoDataDescriptor>
    getSharedMemoryCompressedVideoDataDescriptor(const QnCompressedVideoData* frame);

/**
 * Ensure compressed frame payload is SHM-backed.
 * If source frame is already SHM-backed, returns it as-is.
 * Otherwise allocates SHM buffer and copies payload once.
 */
NX_MEDIA_CORE_API QnCompressedVideoDataPtr ensureCompressedFrameInSharedMemory(
    const QnConstCompressedVideoDataPtr& frame,
    const nx::media::ffmpeg::FfmpegSharedMemoryAllocatorPtr& sharedMemoryAllocator);
