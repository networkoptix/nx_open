#include <gtest/gtest.h>

#include <transcoding/ffmpeg_audio_buffer.h>

class SampleGen
{
public:
    SampleGen(uint32_t sampleCount, uint32_t sampleSize, uint32_t planeCount)
        : size(sampleCount * sampleSize)
        , sampleSize(sampleSize)
        , planeCount(planeCount)
    {
        data.resize(size);
        dataRef.resize(size);
        for (int i = 0; i < size; ++i)
            data[i] = i;
    }
    uint32_t write(uint8_t** buffers, uint32_t sampleCount)
    {
        uint32_t actualSize = std::min<uint64_t>(sampleCount * sampleSize, size - dataOffset);
        for (int i = 0; actualSize && i < planeCount; ++i)
            memcpy(buffers[i], data.data() + dataOffset, actualSize);
        dataOffset += actualSize;
        return actualSize / sampleSize;
    }

    void read(uint8_t** buffers, uint32_t sampleCount)
    {
        ASSERT_TRUE(sampleCount * sampleSize <= size - dataRefOffset);
        for (int i = 0; i < planeCount; ++i)
            memcpy(dataRef.data() + dataRefOffset, buffers[i], sampleCount * sampleSize);
        dataRefOffset += sampleCount * sampleSize;
    }

    uint32_t samplesRest() { return (size - dataOffset) / sampleSize; }

    const uint32_t size;
    std::vector<uint8_t> data;
    std::vector<uint8_t> dataRef;

private:
    const uint32_t sampleSize = 1;
    const uint32_t planeCount = 1;

    uint64_t dataOffset = 0;
    uint64_t dataRefOffset = 0;
};

void testReadWrite(int readCount, int writeCount, int sampleCount)
{
    FfmpegAudioBuffer buffer;
    FfmpegAudioBuffer::Config config {2, AV_SAMPLE_FMT_S16};
    buffer.init(config);
    SampleGen samples(sampleCount, buffer.sampleSize(), buffer.planeCount());

    uint8_t** buffers;
    while (samples.samplesRest() != 0)
    {
        buffers = buffer.startWriting(writeCount);
        ASSERT_TRUE(buffers != nullptr);
        buffer.finishWriting(samples.write(buffers, writeCount));
        while(buffer.sampleCount() >= readCount)
        {
            ASSERT_TRUE(buffer.popData(readCount, buffers));
            samples.read(buffers, readCount);
        }
    }

    if (buffer.sampleCount() > 0)
    {
        int restCount = buffer.sampleCount();
        ASSERT_TRUE(buffer.popData(restCount, buffers));
        samples.read(buffers, restCount);
    }
    ASSERT_TRUE(samples.data == samples.dataRef);
}

TEST(FFmpegAudioFilter, Test)
{
    testReadWrite(33, 64, 1024 * 32);
    testReadWrite(64, 33, 1024);
    testReadWrite(33, 64, 1);
    testReadWrite(1, 1, 77);
    testReadWrite(1, 1, 1);
    testReadWrite(3, 7, 100);
}
