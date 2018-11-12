#include <memory>

#include <gtest/gtest.h>

#include <nx/utils/random.h>

#include <streaming/streaming_chunk.h>

namespace test {

static constexpr std::size_t kMaxChunkBufferSize = 16 * 1024;

class StreamingChunkInputStream: public ::testing::Test
{
protected:
    void givenChunkInputStream()
    {
        givenChunk();

        m_chunkInputStream = std::make_unique<::StreamingChunkInputStream>(m_chunk.get());
    }

    void givenChunk()
    {
        m_chunk = std::make_shared<StreamingChunk>(m_chunkParams, kMaxChunkBufferSize);
        m_chunkData = nx::utils::random::generate(
            nx::utils::random::number<size_t>(1, kMaxChunkBufferSize));
        ASSERT_TRUE(m_chunk->openForModification());
        m_chunk->appendData(m_chunkData);
    }

    void whenReadStream()
    {
        ASSERT_TRUE(m_chunkInputStream->tryRead(&m_readBuffer, kMaxChunkBufferSize));
    }

    void thenChunkDataHasBeenRead()
    {
        ASSERT_EQ(m_chunkData.size(), m_readBuffer.size());
        ASSERT_EQ(m_chunkData, m_readBuffer);
    }

private:
    std::shared_ptr<StreamingChunk> m_chunk;
    std::unique_ptr<::StreamingChunkInputStream> m_chunkInputStream;
    StreamingChunkCacheKey m_chunkParams;
    nx::Buffer m_chunkData;
    nx::Buffer m_readBuffer;
};

TEST_F(StreamingChunkInputStream, reads_stream_from_chunk)
{
    givenChunkInputStream();
    whenReadStream();
    thenChunkDataHasBeenRead();
}

} // namespace test
