#include "ffmpeg_audio_buffer.h"

#include "transcoding_utils.h"
#include <nx/utils/log/log.h>

FfmpegAudioBuffer::~FfmpegAudioBuffer()
{
    releaseBuffers();
}

void FfmpegAudioBuffer::init(const Config& config)
{
    m_config = config;
    bool planar = av_sample_fmt_is_planar(m_config.sampleFormat);
    m_planeCount =  planar ? m_config.channelCount : 1;
    m_sampleSize = av_get_bytes_per_sample(m_config.sampleFormat)
        * (planar ? 1 : m_config.channelCount);
    m_temporaryBufferPointers.resize(m_planeCount);
}

uint8_t** FfmpegAudioBuffer::startWriting(uint64_t sampleCount)
{
    uint64_t requestedSize = m_sampleSize * sampleCount;
    if (m_sampleBuffers == nullptr || requestedSize > m_bufferSize - m_dataSize)
    {
        if (!allocBuffers(sampleCount + m_dataSize / m_sampleSize))
            return nullptr;
    }

    if (requestedSize > m_bufferSize - m_dataSize - m_dataOffset)
        moveDataToStart();

    for (uint32_t i = 0; i < m_planeCount; ++i)
        m_temporaryBufferPointers[i] = m_sampleBuffers[i] + m_dataOffset + m_dataSize;

    return m_temporaryBufferPointers.data();
}

void FfmpegAudioBuffer::finishWriting(uint64_t sampleCount)
{
    m_dataSize += m_sampleSize * sampleCount;
}

bool FfmpegAudioBuffer::popData(uint64_t sampleCount, uint8_t** buffers)
{
    uint64_t requestedSize = m_sampleSize * sampleCount;
    if (m_sampleBuffers == nullptr || requestedSize > m_dataSize)
        return false;

    for (uint32_t i = 0; i < m_planeCount; ++i)
        buffers[i] = m_sampleBuffers[i] + m_dataOffset;

    m_dataOffset += requestedSize;
    m_dataSize -= requestedSize;
    return true;
}

bool FfmpegAudioBuffer::allocBuffers(uint64_t sampleCount)
{
    int linesize = 0;
    uint8_t** buffers;
    auto result = av_samples_alloc_array_and_samples(
        &buffers,
        &linesize,
        m_config.channelCount,
        sampleCount,
        m_config.sampleFormat,
        0/*align*/);

    if (result < 0)
    {
        NX_ERROR(this, "Failed to allocate resample buffer, error: %1",
            nx::transcoding::ffmpegErrorText(result));
        return false;
    }
    if (m_sampleBuffers)
    {
        // if data already keep, copy it to new buffers
        if (m_dataSize > 0)
        {
            for (uint32_t i = 0; i < m_planeCount; ++i)
                memcpy(buffers[i], m_sampleBuffers[i] + m_dataOffset, m_dataSize);
        }
        releaseBuffers();
    }
    m_sampleBuffers = buffers;
    m_bufferSize = linesize;
    m_dataOffset = 0;
    return true;
}

void FfmpegAudioBuffer::releaseBuffers()
{
    if (m_sampleBuffers)
    {
        // av_samples_alloc_array_and_samples does two av_allocs: buffer for pointers and
        // payload data. It requires two free calls.
        av_freep(m_sampleBuffers);
        av_freep(&m_sampleBuffers);
    }
}

void FfmpegAudioBuffer::moveDataToStart()
{
    for (uint32_t i = 0; i < m_planeCount; ++i)
        memmove(m_sampleBuffers[i], m_sampleBuffers[i] + m_dataOffset, m_dataSize);

    m_dataOffset = 0;
}
