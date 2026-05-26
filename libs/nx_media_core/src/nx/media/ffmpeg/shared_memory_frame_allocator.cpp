// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "shared_memory_frame_allocator.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <ranges>
#include <string>
#include <utility>

#if defined(_WIN32)
#include <boost/interprocess/managed_windows_shared_memory.hpp>
#else
#include <boost/interprocess/managed_shared_memory.hpp>
#endif

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/buffer.h>
#include <libavutil/cpu.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
} // extern "C"

#include <nx/ranges/std_compat.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>

namespace nx::media::ffmpeg {

namespace {

using ShmHandle = std::uintptr_t;
#if defined(_WIN32)
using SharedMemorySegment = boost::interprocess::managed_windows_shared_memory;
#else
using SharedMemorySegment = boost::interprocess::managed_shared_memory;
#endif

// Missing AVCodecContext SHM wiring can happen in misconfigured call paths.
// Log once to keep logs actionable without flooding during high frame throughput.
std::once_flag gMissingBufferContextWarningOnce;

constexpr std::size_t kMinSharedMemoryAlignment = 64;

struct SharedMemoryBuffer
{
    void* data = nullptr;
    std::uintptr_t handle = 0;
};

std::size_t sharedMemoryAlignment()
{
    // FFmpeg codecs may use wide SIMD loads on decoded planes. Keep SHM allocations aligned to
    // at least av_cpu_max_align() and never below 64 bytes.
    const std::size_t ffmpegAlignment = std::max<std::size_t>(
        static_cast<std::size_t>(av_cpu_max_align()),
        alignof(std::max_align_t));
    return std::max(ffmpegAlignment, kMinSharedMemoryAlignment);
}

struct AvFrameDeleter
{
    void operator()(AVFrame* frame) const
    {
        av_frame_free(&frame);
    }
};

class FfmpegSharedMemoryAllocatorImpl final: public FfmpegSharedMemoryAllocator
{
public:
    FfmpegSharedMemoryAllocatorImpl(std::string segmentName, std::size_t segmentSizeBytes):
        m_segmentName(std::move(segmentName)),
        m_segment(std::make_unique<SharedMemorySegment>(
            boost::interprocess::create_only,
            m_segmentName.c_str(),
            segmentSizeBytes))
    {
    }

    ~FfmpegSharedMemoryAllocatorImpl() override
    {
        // Serialize shutdown with runtime allocations/deallocations.
        std::lock_guard<std::mutex> lock(m_mutex);

        // Destroy the mapped segment first so no further operations are possible.
        m_segment.reset();

#if !defined(_WIN32)
        // Unlink the named segment on graceful shutdown.
        boost::interprocess::shared_memory_object::remove(m_segmentName.c_str());
#endif
    }

    std::string_view segmentName() const override
    {
        return m_segmentName;
    }

    void* allocate(std::size_t size) override
    {
        // Boost managed_shared_memory allocation/deallocation is not thread-safe.
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_segment)
            return nullptr;

        void* ptr = nullptr;
        try
        {
            ptr = m_segment->allocate_aligned(size, sharedMemoryAlignment());
        }
        catch (const boost::interprocess::bad_alloc&)
        {
            NX_WARNING(
                NX_SCOPE_TAG,
                "Failed to allocate shared memory block of size %1 in segment %2 "
                "(boost::interprocess::bad_alloc)",
                size,
                m_segmentName);
            return nullptr;
        }

        // Track active blocks for diagnostics and to reject invalid deallocation calls.
        m_allocations[ptr] = size;
        logAllocationStatsLocked("New block allocated");
        return ptr;
    }

    void deallocate(void* ptr) override
    {
        if (!ptr)
            return;

        // Protect allocation map and segment operations against concurrent access.
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_segment)
            return;

        const auto it = m_allocations.find(ptr);
        if (it == m_allocations.end())
        {
            NX_WARNING(
                NX_SCOPE_TAG,
                "Attempt to deallocate unknown shared memory block %1 from segment %2",
                ptr,
                m_segmentName);
            return;
        }

        // Remove tracking first, then release memory back to the segment.
        m_allocations.erase(it);
        logAllocationStatsLocked("Block de-allocated");
        m_segment->deallocate(ptr);
    }

    bool contains(const void* ptr) const override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_segment && ptr && m_segment->belongs_to_segment(ptr);
    }

    std::optional<std::uintptr_t> handleFromAddress(const void* ptr) const override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_segment || !ptr)
            return std::nullopt;

        // Only addresses that belong to this segment can be represented as SHM handles.
        if (!m_segment->belongs_to_segment(ptr))
            return std::nullopt;

        return static_cast<ShmHandle>(m_segment->get_handle_from_address(ptr));
    }

private:
    void logAllocationStatsLocked(const char* message) const
    {
        if (!nx::log::isToBeLogged(nx::log::Level::verbose))
            return;

        const std::size_t totalAllocatedSize =
            nx::ranges::fold_left(
                m_allocations | std::views::values,
                std::size_t{0},
                std::plus<>{});

        NX_VERBOSE(
            NX_SCOPE_TAG,
            "%1. Segment: %2. Current allocations count: %3, current allocations size: %4",
            message,
            m_segmentName,
            m_allocations.size(),
            totalAllocatedSize);
    }

private:
    const std::string m_segmentName;
    std::unique_ptr<SharedMemorySegment> m_segment;
    std::map<void*, std::size_t> m_allocations;
    mutable std::mutex m_mutex;
};

struct SharedMemoryBufferOpaque
{
    static constexpr std::uint64_t kMagic = 0x53484D4255464F50ULL; //< "SHMBUFOP".

    std::uint64_t magic = kMagic;
    FfmpegSharedMemoryAllocatorPtr allocator;
};

void releaseSharedMemoryBuffer(void* opaque, uint8_t* data)
{
    std::unique_ptr<SharedMemoryBufferOpaque> lifetime(
        static_cast<SharedMemoryBufferOpaque*>(opaque));

    if (!lifetime || !lifetime->allocator)
        return;

    // AVBufferRef lifetime ended; return this block to the shared-memory segment.
    lifetime->allocator->deallocate(data);
}

AVBufferRef* createSharedMemoryBufferRef(
    uint8_t* data,
    int size,
    const FfmpegSharedMemoryAllocatorPtr& allocator)
{
    if (!data || size <= 0 || !allocator)
        return nullptr;

    auto lifetime = std::make_unique<SharedMemoryBufferOpaque>();
    lifetime->allocator = allocator;

    AVBufferRef* bufferRef = av_buffer_create(
        data,
        size,
        releaseSharedMemoryBuffer,
        lifetime.get(),
        0);

    if (!bufferRef)
        return nullptr;

    lifetime.release();
    return bufferRef;
}

FfmpegSharedMemoryAllocatorPtr allocatorFromBufferRef(const AVBufferRef* bufferRef)
{
    if (!bufferRef || !bufferRef->data)
        return {};

    const auto* const opaque = static_cast<const SharedMemoryBufferOpaque*>(
        av_buffer_get_opaque(bufferRef));
    if (!opaque || opaque->magic != SharedMemoryBufferOpaque::kMagic || !opaque->allocator)
        return {};

    if (!opaque->allocator->contains(bufferRef->data))
        return {};

    return opaque->allocator;
}

const FfmpegSharedMemoryBufferContext* bufferContextFromCodecContext(const AVCodecContext* codecCtx)
{
    if (!codecCtx || !codecCtx->opaque)
        return nullptr;

    const auto* const context =
        static_cast<const FfmpegSharedMemoryBufferContext*>(codecCtx->opaque);
    if (context->magic != FfmpegSharedMemoryBufferContext::kMagic)
        return nullptr;

    return context;
}

FfmpegSharedMemoryAllocatorPtr allocatorFromCodecContext(const AVCodecContext* codecCtx)
{
    if (const auto* const context = bufferContextFromCodecContext(codecCtx))
        return context->allocator;

    std::call_once(gMissingBufferContextWarningOnce, [codecCtx]()
    {
        NX_WARNING(
            NX_SCOPE_TAG,
            "Missing/invalid SHM buffer context in AVCodecContext::opaque (codecCtx=%1). "
            "Cannot allocate SHM frame without explicit allocator",
            codecCtx);
    });

    return {};
}

void assignFrameStorage(
    AVFrame* outFrame,
    const std::array<uint8_t*, AV_NUM_DATA_POINTERS>& data,
    const std::array<int, AV_NUM_DATA_POINTERS>& linesize,
    const std::array<AVBufferRef*, AV_NUM_DATA_POINTERS>& buffers)
{
    for (int i = 0; i < AV_NUM_DATA_POINTERS; ++i)
    {
        outFrame->data[i] = data[i];
        outFrame->linesize[i] = linesize[i];
        outFrame->buf[i] = buffers[i];
    }
    outFrame->extended_data = outFrame->data;
}

int allocateSharedMemoryFrameFromLayout(
    const AVFrame* layoutFrame,
    AVFrame* outFrame,
    const FfmpegSharedMemoryAllocatorPtr& allocator)
{
    if (!layoutFrame || !outFrame || !allocator)
        return AVERROR(EINVAL);

    std::array<uint8_t*, AV_NUM_DATA_POINTERS> data = {};
    std::array<int, AV_NUM_DATA_POINTERS> linesize = {};
    std::array<AVBufferRef*, AV_NUM_DATA_POINTERS> buffers = {};

    auto releaseBuffersGuard = nx::utils::makeScopeGuard([&buffers]()
    {
        for (auto& bufferRef: buffers)
            av_buffer_unref(&bufferRef);
    });

    // For each source AVBufferRef from the layout frame, allocate an equivalent SHM block and
    // wrap it into AVBufferRef so FFmpeg can own/release it normally.
    for (int sourceBufferIndex = 0; sourceBufferIndex < AV_NUM_DATA_POINTERS; ++sourceBufferIndex)
    {
        AVBufferRef* const sourceBuffer = layoutFrame->buf[sourceBufferIndex];
        if (!sourceBuffer)
        {
            // Per FFmpeg AVFrame contract, buf[] is densely packed from index 0:
            // after the first null entry, all following entries are null too.
            break;
        }

        void* sharedMemoryBuffer = allocator->allocate(sourceBuffer->size);
        if (!sharedMemoryBuffer)
            return AVERROR(ENOMEM);

        AVBufferRef* targetBuffer = createSharedMemoryBufferRef(
            static_cast<uint8_t*>(sharedMemoryBuffer),
            sourceBuffer->size,
            allocator);
        if (!targetBuffer)
        {
            // AVBufferRef creation failed, return this block to the shared-memory segment.
            allocator->deallocate(sharedMemoryBuffer);
            return AVERROR(ENOMEM);
        }

        buffers[sourceBufferIndex] = targetBuffer;
    }

    // Rebuild each plane pointer by finding which original buffer it referenced and applying
    // the same byte offset within the corresponding SHM buffer.
    for (int plane = 0; plane < AV_NUM_DATA_POINTERS; ++plane)
    {
        if (!layoutFrame->data[plane])
            continue;

        int sourceBufferIndex = -1;
        ptrdiff_t dataOffset = -1;

        for (int i = 0; i < AV_NUM_DATA_POINTERS; ++i)
        {
            AVBufferRef* const sourceBuffer = layoutFrame->buf[i];
            if (!sourceBuffer)
            {
                // Same AVFrame buf[] rule: first null means there are no further buffers.
                break;
            }

            uint8_t* const sourceBufferStart = sourceBuffer->data;
            uint8_t* const sourceBufferEnd = sourceBufferStart + sourceBuffer->size;
            if (layoutFrame->data[plane] >= sourceBufferStart
                && layoutFrame->data[plane] < sourceBufferEnd)
            {
                sourceBufferIndex = i;
                dataOffset = layoutFrame->data[plane] - sourceBufferStart;
                break;
            }
        }

        if (sourceBufferIndex < 0 || dataOffset < 0)
            return AVERROR(EINVAL);

        AVBufferRef* const targetBuffer = buffers[sourceBufferIndex];
        data[plane] = targetBuffer->data + dataOffset;
        linesize[plane] = layoutFrame->linesize[plane];
    }

    assignFrameStorage(outFrame, data, linesize, buffers);
    releaseBuffersGuard.disarm();
    return 0;
}

std::optional<SharedMemoryBuffer> allocateBytesInSharedMemory(
    std::size_t size,
    const FfmpegSharedMemoryAllocatorPtr& allocator)
{
    if (!allocator || size == 0)
        return std::nullopt;

    void* data = allocator->allocate(size);
    if (!data)
        return std::nullopt;

    const auto handle = allocator->handleFromAddress(data);
    if (!handle)
    {
        allocator->deallocate(data);
        return std::nullopt;
    }

    return SharedMemoryBuffer{data, *handle};
}

} // namespace

FfmpegSharedMemoryAllocatorPtr createFfmpegSharedMemoryAllocator(
    std::string segmentName,
    std::size_t segmentSizeBytes)
{
    if (segmentName.empty() || segmentSizeBytes == 0)
    {
        NX_WARNING(
            NX_SCOPE_TAG,
            "Failed to create shared memory allocator: invalid args (name=%1 size=%2)",
            segmentName,
            segmentSizeBytes);
        return {};
    }

    const std::string segmentNameForLog = segmentName;

    try
    {
        auto allocator = std::make_shared<FfmpegSharedMemoryAllocatorImpl>(
            std::move(segmentName),
            segmentSizeBytes);
        return allocator;
    }
    catch (const boost::interprocess::interprocess_exception& e)
    {
        NX_WARNING(
            NX_SCOPE_TAG,
            "Failed to create shared memory allocator for segment %1: %2",
            segmentNameForLog,
            e.what());
    }
    catch (const std::exception& e)
    {
        NX_WARNING(
            NX_SCOPE_TAG,
            "Failed to create shared memory allocator for segment %1: %2",
            segmentNameForLog,
            e.what());
    }

    return {};
}

int getFfmpegSharedMemoryBuffer(AVCodecContext* codecCtx, AVFrame* frame, int flags)
{
    auto defaultFrame = std::unique_ptr<AVFrame, AvFrameDeleter>(
        av_frame_alloc(),
        AvFrameDeleter{});
    if (!defaultFrame)
        return AVERROR(ENOMEM);

    defaultFrame->format = frame->format >= 0 ? frame->format : codecCtx->pix_fmt;
    defaultFrame->width = frame->width > 0 ? frame->width : codecCtx->width;
    defaultFrame->height = frame->height > 0 ? frame->height : codecCtx->height;

    // Ask FFmpeg for canonical plane layout, offsets and alignment for this codec context.
    const int defaultBufferResult = avcodec_default_get_buffer2(codecCtx, defaultFrame.get(), flags);
    if (defaultBufferResult < 0)
        return defaultBufferResult;

    // Keep this layout-based path: FFmpeg may choose codec-specific plane offsets/buffers here,
    // and we must mirror that exact AVFrame memory shape in shared memory.
    return allocateSharedMemoryFrameFromLayout(defaultFrame.get(), frame, allocatorFromCodecContext(codecCtx));
}

CLVideoDecoderOutputPtr allocateSystemFrameInSharedMemory(
    int width,
    int height,
    AVPixelFormat pixelFormat,
    const FfmpegSharedMemoryAllocatorPtr& allocator)
{
    if (!allocator)
        return nullptr;

    CLVideoDecoderOutputPtr result(new CLVideoDecoderOutput());
    result->width = width;
    result->height = height;
    result->format = pixelFormat;

    const int bufferSize = av_image_get_buffer_size(
        pixelFormat,
        width,
        height,
        CL_MEDIA_ALIGNMENT);
    if (bufferSize < 0)
        return nullptr;

    void* sharedMemoryBuffer = allocator->allocate(bufferSize);
    if (!sharedMemoryBuffer)
        return nullptr;

    AVBufferRef* bufferRef = createSharedMemoryBufferRef(
        static_cast<uint8_t*>(sharedMemoryBuffer),
        bufferSize,
        allocator);
    if (!bufferRef)
    {
        allocator->deallocate(sharedMemoryBuffer);
        return nullptr;
    }

    std::array<uint8_t*, AV_NUM_DATA_POINTERS> data = {};
    std::array<int, AV_NUM_DATA_POINTERS> linesize = {};
    std::array<AVBufferRef*, AV_NUM_DATA_POINTERS> buffers = {};

    const int fillResult = av_image_fill_arrays(
        data.data(),
        linesize.data(),
        static_cast<uint8_t*>(sharedMemoryBuffer),
        pixelFormat,
        width,
        height,
        CL_MEDIA_ALIGNMENT);
    if (fillResult < 0)
    {
        av_buffer_unref(&bufferRef);
        return nullptr;
    }

    buffers[0] = bufferRef;
    assignFrameStorage(result.get(), data, linesize, buffers);
    return result;
}

FfmpegSharedMemoryAllocatorPtr ffmpegSharedMemoryAllocatorFromFrame(const AVFrame* frame)
{
    if (!frame)
        return {};

    FfmpegSharedMemoryAllocatorPtr allocator;

    for (int bufferIndex = 0; bufferIndex < AV_NUM_DATA_POINTERS; ++bufferIndex)
    {
        const AVBufferRef* const buffer = frame->buf[bufferIndex];
        if (!buffer)
            break;

        const auto currentAllocator = allocatorFromBufferRef(buffer);
        if (!currentAllocator)
            return {};

        if (!allocator)
            allocator = currentAllocator;
        else if (allocator.get() != currentAllocator.get())
            return {};
    }

    if (!allocator)
        return {};

    bool hasAnyData = false;
    for (int plane = 0; plane < AV_NUM_DATA_POINTERS; ++plane)
    {
        if (!frame->data[plane])
            continue;

        hasAnyData = true;
        // Every non-null plane must be resolvable to this SHM segment.
        if (!allocator->contains(frame->data[plane]))
            return {};
    }

    return hasAnyData ? allocator : FfmpegSharedMemoryAllocatorPtr{};
}

std::string ffmpegSharedMemorySegmentNameFromFrame(const AVFrame* frame)
{
    if (const auto allocator = ffmpegSharedMemoryAllocatorFromFrame(frame))
        return std::string(allocator->segmentName());

    return {};
}

bool isFrameBackedBySharedMemory(const AVFrame* frame)
{
    return static_cast<bool>(ffmpegSharedMemoryAllocatorFromFrame(frame));
}

std::optional<std::uintptr_t> ffmpegSharedMemoryHandleFromAddress(
    const void* address,
    const FfmpegSharedMemoryAllocatorPtr& allocator)
{
    if (!allocator)
        return std::nullopt;

    return allocator->handleFromAddress(address);
}

std::optional<nx::utils::ShmByteArray> allocateShmByteArrayInSharedMemory(
    std::size_t size,
    const FfmpegSharedMemoryAllocatorPtr& allocator,
    std::size_t trailingZeroPadding)
{
    if (!allocator || size == 0)
        return std::nullopt;

    if (trailingZeroPadding > std::numeric_limits<std::size_t>::max() - size)
        return std::nullopt;

    const std::size_t allocationSize = size + trailingZeroPadding;
    const auto shmBuffer = allocateBytesInSharedMemory(allocationSize, allocator);
    if (!shmBuffer)
        return std::nullopt;

    if (trailingZeroPadding > 0)
    {
        std::memset(
            static_cast<char*>(shmBuffer->data) + size,
            0,
            trailingZeroPadding);
    }

    std::string segmentName(allocator->segmentName());
    auto lifetimeGuard = std::shared_ptr<void>(
        shmBuffer->data,
        [allocator](void* data)
        {
            allocator->deallocate(data);
        });

    return nx::utils::detail::makeShmByteArray(
        shmBuffer->data,
        size,
        shmBuffer->handle,
        std::move(segmentName),
        std::move(lifetimeGuard));
}

} // namespace nx::media::ffmpeg
