#pragma once

extern "C" {
#include <libavformat/avformat.h>
}

#include <vector>

/**
 * Audio sample buffer, that help to implement non copy audio resampling
 */
class FfmpegAudioBuffer
{
public:
    struct Config
    {
        int32_t channelCount = 0;
        AVSampleFormat sampleFormat = AV_SAMPLE_FMT_NONE;
    };

public:
    ~FfmpegAudioBuffer();

    void init(const Config& config);

    /**
     * Reserves space in this buffer to write given sample count and returns a pointer to the
     * reserved memory. This function is to be used when some external mechanism is employed for
     * writing into memory. 'finishWriting()' must be called after external writing operation is
     * complete.
     *
     * @param sampleCount Number of samples to reserve for writing.
     * @returns Pointer to the reserved memory.
     */
    uint8_t** startWriting(uint64_t sampleCount);

    /**
     * @param sampleCount Number of samples that were appended to this
     * array using external mechanisms.
     */
    void finishWriting(uint64_t sampleCount);

    /**
     * Fills the buffers array by pointers to internal buffers with samples data
     * @return true if buffer contain requested sampleCount, otherwise false.
     */
    bool popData(uint64_t sampleCount, uint8_t** buffers);

    uint32_t sampleCount() const { return m_dataSize / m_sampleSize; }
    uint32_t planeCount() const { return m_planeCount; }
    uint32_t sampleSize() const { return m_sampleSize; }

private:
    bool allocBuffers(uint64_t sampleCount);
    void releaseBuffers();
    void moveDataToStart();

private:
    Config m_config;
    uint64_t m_bufferSize = 0;
    uint64_t m_dataOffset = 0;
    uint64_t m_dataSize = 0;
    uint32_t m_planeCount = 0;
    uint32_t m_sampleSize = 0;
    uint8_t** m_sampleBuffers = nullptr;
    std::vector<uint8_t*> m_temporaryBufferPointers;
};
