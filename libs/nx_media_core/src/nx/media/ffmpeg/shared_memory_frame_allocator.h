// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <nx/media/ffmpeg/frame_info.h>

extern "C" {
#include <libavutil/pixfmt.h>
} // extern "C"

struct AVCodecContext;
struct AVFrame;

namespace nx::media::ffmpeg {

class NX_MEDIA_CORE_API FfmpegSharedMemoryAllocator
{
public:
    virtual ~FfmpegSharedMemoryAllocator() = default;

    virtual std::string_view segmentName() const = 0;
    virtual void* allocate(std::size_t size) = 0;
    virtual void deallocate(void* ptr) = 0;
    virtual bool contains(const void* ptr) const = 0;
    virtual std::optional<std::uintptr_t> handleFromAddress(const void* ptr) const = 0;
};

using FfmpegSharedMemoryAllocatorPtr = std::shared_ptr<FfmpegSharedMemoryAllocator>;

struct NX_MEDIA_CORE_API FfmpegSharedMemoryBufferContext
{
    static constexpr std::uint64_t kMagic = 0x4653464243465854ULL; //< "FSFBCFXT".

    std::uint64_t magic = kMagic;
    void* owner = nullptr; //< Optional owner pointer used by decoder-specific callbacks.
    FfmpegSharedMemoryAllocatorPtr allocator;
};

/**
 * Create a named shared-memory allocator instance.
 * Returns null on name collision/open failure.
 */
NX_MEDIA_CORE_API FfmpegSharedMemoryAllocatorPtr createFfmpegSharedMemoryAllocator(
    std::string segmentName,
    std::size_t segmentSizeBytes);

/**
 * FFmpeg get_buffer2 callback that allocates frame planes in FfmpegSharedMemory.
 */
NX_MEDIA_CORE_API int getFfmpegSharedMemoryBuffer(
    AVCodecContext* codecCtx,
    AVFrame* frame,
    int flags);

/**
 * Allocate a software AVFrame layout in a specific shared-memory segment.
 */
NX_MEDIA_CORE_API CLVideoDecoderOutputPtr allocateSystemFrameInSharedMemory(
    int width,
    int height,
    AVPixelFormat pixelFormat,
    const FfmpegSharedMemoryAllocatorPtr& allocator);

/**
 * @return true if all non-empty plane pointers belong to FfmpegSharedMemory.
 */
NX_MEDIA_CORE_API bool isFrameBackedBySharedMemory(const AVFrame* frame);

/**
 * Returns allocator associated with the AVFrame buffers, or null if frame is not SHM-backed.
 */
NX_MEDIA_CORE_API FfmpegSharedMemoryAllocatorPtr ffmpegSharedMemoryAllocatorFromFrame(
    const AVFrame* frame);

/**
 * Returns segment name for SHM-backed AVFrame buffers, or empty string otherwise.
 */
NX_MEDIA_CORE_API std::string ffmpegSharedMemorySegmentNameFromFrame(const AVFrame* frame);

/**
 * Resolve address to SHM handle in the provided allocator mapping.
 */
NX_MEDIA_CORE_API std::optional<std::uintptr_t> ffmpegSharedMemoryHandleFromAddress(
    const void* address,
    const FfmpegSharedMemoryAllocatorPtr& allocator);

} // namespace nx::media::ffmpeg
