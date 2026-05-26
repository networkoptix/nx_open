// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "video_data_packet.h"

#include <cstring>

#include <nx/media/ffmpeg/shared_memory_frame_allocator.h>
#include <nx/utils/log/log.h>

QnCompressedVideoData::QnCompressedVideoData( CodecParametersConstPtr ctx )
:
    QnAbstractMediaData( VIDEO ),
    width( -1 ),
    height( -1 ),
    pts( AV_NOPTS_VALUE )
{
    //useTwice = false;
    context = ctx;
    //ignore = false;
    flags = {};
}

void QnCompressedVideoData::assign(const QnCompressedVideoData* other)
{
    QnAbstractMediaData::assign(other);
    width = other->width;
    height = other->height;
    metadata = other->metadata;
    pts = other->pts;
}

QnWritableCompressedVideoData::QnWritableCompressedVideoData(
    size_t capacity,
    CodecParametersConstPtr ctx)
    :
    QnCompressedVideoData(ctx),
    m_data(CL_MEDIA_ALIGNMENT, capacity, AV_INPUT_BUFFER_PADDING_SIZE)
{
}

//!Implementation of QnAbstractMediaData::clone
QnWritableCompressedVideoData* QnWritableCompressedVideoData::clone() const
{
    auto rez = new QnWritableCompressedVideoData();
    rez->assign(this);
    return rez;
}

//!Implementation of QnAbstractMediaData::data
const char* QnWritableCompressedVideoData::data() const
{
    return m_data.data();
}

//!Implementation of QnAbstractMediaData::dataSize
size_t QnWritableCompressedVideoData::dataSize() const
{
    return m_data.size();
}

void QnWritableCompressedVideoData::setData(nx::utils::ByteArray&& buffer)
{
    m_data = std::move(buffer);
}

void QnWritableCompressedVideoData::assign(const QnWritableCompressedVideoData* other)
{
    QnCompressedVideoData::assign(other);
    m_data = other->m_data;
}

QnSharedMemoryCompressedVideoData::QnSharedMemoryCompressedVideoData(
    nx::utils::ShmByteArray data,
    CodecParametersConstPtr ctx):
    QnCompressedVideoData(ctx),
    m_data(std::move(data))
{
}

QnWritableCompressedVideoData* QnSharedMemoryCompressedVideoData::clone() const
{
    // Clone must return writable packet, so SHM payload is copied to RAM-backed storage.
    auto cloned = new QnWritableCompressedVideoData();
    cloned->QnCompressedVideoData::assign(this);
    cloned->m_data.write(m_data.constData(), m_data.size());
    return cloned;
}

const char* QnSharedMemoryCompressedVideoData::data() const
{
    return m_data.constData();
}

size_t QnSharedMemoryCompressedVideoData::dataSize() const
{
    return m_data.size();
}

void QnSharedMemoryCompressedVideoData::setData(nx::utils::ByteArray&& /*buffer*/)
{
    NX_ASSERT(false, "setData() is not supported for SHM-backed compressed frames");
}

char* QnSharedMemoryCompressedVideoData::mutableData()
{
    return m_data.data();
}

const nx::utils::ShmByteArray& QnSharedMemoryCompressedVideoData::shmData() const
{
    return m_data;
}

QnSharedMemoryCompressedVideoDataPtr createSharedMemoryCompressedVideoData(
    size_t dataSize,
    const nx::media::ffmpeg::FfmpegSharedMemoryAllocatorPtr& sharedMemoryAllocator,
    CodecParametersConstPtr ctx)
{
    const auto shmData = nx::media::ffmpeg::allocateShmByteArrayInSharedMemory(
        dataSize,
        sharedMemoryAllocator,
        AV_INPUT_BUFFER_PADDING_SIZE);
    if (!shmData)
        return {};

    return std::make_shared<QnSharedMemoryCompressedVideoData>(std::move(*shmData), std::move(ctx));
}

bool isCompressedFrameInSharedMemory(const QnCompressedVideoData* frame)
{
    return dynamic_cast<const QnSharedMemoryCompressedVideoData*>(frame) != nullptr;
}

std::optional<SharedMemoryCompressedVideoDataDescriptor> getSharedMemoryCompressedVideoDataDescriptor(
    const QnCompressedVideoData* frame)
{
    const auto* shmFrame = dynamic_cast<const QnSharedMemoryCompressedVideoData*>(frame);
    if (!shmFrame)
        return std::nullopt;

    const auto& shmData = shmFrame->shmData();
    return SharedMemoryCompressedVideoDataDescriptor{
        .data = shmData.constData(),
        .dataSize = shmData.size(),
        .sharedMemoryHandle = shmData.handle(),
        .sharedMemoryName = std::string(shmData.segmentName()),
    };
}

QnCompressedVideoDataPtr ensureCompressedFrameInSharedMemory(
    const QnConstCompressedVideoDataPtr& frame,
    const nx::media::ffmpeg::FfmpegSharedMemoryAllocatorPtr& sharedMemoryAllocator)
{
    if (!frame)
        return {};

    if (isCompressedFrameInSharedMemory(frame.get()))
        return std::const_pointer_cast<QnCompressedVideoData>(frame);

    const char* sourceData = frame->data();
    const size_t sourceDataSize = frame->dataSize();
    if (sourceDataSize == 0)
    {
        NX_WARNING(
            NX_SCOPE_TAG,
            "Failed to place compressed frame into shared memory: empty payload");
        return {};
    }
    if (!sourceData)
    {
        NX_WARNING(
            NX_SCOPE_TAG,
            "Failed to place compressed frame into shared memory: dataSize=%1 but data pointer is null",
            sourceDataSize);
        return {};
    }

    auto result = createSharedMemoryCompressedVideoData(
        sourceDataSize,
        sharedMemoryAllocator,
        frame->context);
    if (!result)
    {
        NX_WARNING(
            NX_SCOPE_TAG,
            "Failed to allocate %1 bytes in shared memory for compressed frame",
            sourceDataSize);
        return {};
    }

    result->QnCompressedVideoData::assign(frame.get());
    std::memcpy(result->mutableData(), sourceData, sourceDataSize);

    return result;
}
