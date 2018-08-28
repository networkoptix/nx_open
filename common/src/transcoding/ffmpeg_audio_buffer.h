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
     * @return return array of buffers(allocated internally) to write sample data or nullptr when
     * error occured.
     */
    uint8_t** getBuffer(uint64_t sampleCount);

    /**
     * Confirm written sample count.
     */
    void commit(uint64_t sampleCount);

    /**
     * Fill the buffers array by pointers to internal buffers with sample data
     * @return true if buffer contain requested sampleCount, otherwise false.
     */
    bool getData(uint64_t sampleCount, uint8_t** buffers);

    uint32_t sampleCount() const { return m_dataSize / m_sampleSize; }
    uint32_t planeCount() const { return m_planeCount; }
    uint32_t sampleSize() const { return m_sampleSize; }

private:
    bool allocBuffers(uint64_t sampleCount);
    void releaseBuffers();
    void moveDataToStart();

private:
    Config m_config;
    uint64_t m_size = 0;
    uint64_t m_dataOffset = 0;
    uint64_t m_dataSize = 0;
    uint32_t m_planeCount = 0;
    uint32_t m_sampleSize = 0;
    uint8_t** m_sampleBuffers = nullptr;
    std::vector<uint8_t*> m_temporaryBufferPointers;
};
