#include <gtest/gtest.h>

#include <nx/utils/std/cpp14.h>

#include <common/common_module.h>
#include <streaming/streaming_chunk_cache.h>
#include <streaming/streaming_chunk_provider.h>

namespace test {

static const std::chrono::seconds kCacheSize(71);
static const std::size_t kMaxChunkBufferSize = 1024;

namespace {

class StreamingChunkProvider:
    public AbstractStreamingChunkProvider
{
public:
    virtual bool get(
        const StreamingChunkCacheKey& key,
        int* const itemCost,
        StreamingChunkPtr* const chunk) override
    {
        using namespace std::chrono;

        *itemCost = duration_cast<seconds>(key.duration()).count();

        *chunk = std::make_shared<StreamingChunk>(key, kMaxChunkBufferSize);
        return true;
    }
};

} // namespace

class StreamingChunkCache:
    public ::testing::Test
{
public:
    StreamingChunkCache()
    {
        m_factoryFunctionBak = 
            StreamingChunkProviderFactory::instance().setCustomFunc(
                std::bind(&StreamingChunkCache::createStreamingChunkProvider, this));

        m_streamingChunkCache = 
            std::make_unique<::StreamingChunkCache>(nullptr, nullptr, kCacheSize);
    }

    ~StreamingChunkCache()
    {
        StreamingChunkProviderFactory::instance().setCustomFunc(
            std::move(m_factoryFunctionBak));
    }

protected:
    void givenReadyChunk()
    {
        createChunk(kCacheSize / 2);
    }

    void givenReadyChunkLargerThanCache()
    {
        createChunk(kCacheSize * 2);
    }

    void whenChunkHasBeenTruncatedFromTheBeginning()
    {
        StreamingChunkPtr streamingChunk;
        ASSERT_NE(nullptr, m_streamingChunkCache->getChunkForReading(m_chunkKey, &streamingChunk));
        ASSERT_TRUE(streamingChunk->openForModification());
        streamingChunk->appendData(nx::Buffer(2*kMaxChunkBufferSize, 'x'));

        readBytesFromChunk(streamingChunk, kMaxChunkBufferSize);
    }

    void thenSameChunkIsReturnedByCache()
    {
        StreamingChunkPtr streamingChunk;
        ASSERT_NE(nullptr, m_streamingChunkCache->getChunkForReading(m_chunkKey, &streamingChunk));
        ASSERT_EQ(m_readyChunk, streamingChunk);
    }

    void thenNewChunkIsReturnedByCache()
    {
        StreamingChunkPtr streamingChunk;
        ASSERT_NE(nullptr, m_streamingChunkCache->getChunkForReading(m_chunkKey, &streamingChunk));
        ASSERT_NE(nullptr, streamingChunk);
        ASSERT_NE(m_readyChunk, streamingChunk);
    }

    void thenOriginalChunkHasBeenRemovedFromCache()
    {
        ASSERT_TRUE(m_readyChunk.unique());
    }

private:
    std::unique_ptr<::StreamingChunkCache> m_streamingChunkCache;
    StreamingChunkProviderFactory::Function m_factoryFunctionBak;
    StreamingChunkCacheKey m_chunkKey;
    StreamingChunkPtr m_readyChunk;

    void createChunk(std::chrono::microseconds duration)
    {
        m_chunkKey = StreamingChunkCacheKey(
            "uniqueResourceID",
            0,
            "video/mp2t",
            "alias",
            0,
            duration,
            MediaQuality::MEDIA_Quality_High,
            {});

        StreamingChunkPtr streamingChunk;
        ASSERT_NE(nullptr, m_streamingChunkCache->getChunkForReading(m_chunkKey, &streamingChunk));
        ASSERT_NE(nullptr, streamingChunk);
        ASSERT_TRUE(streamingChunk->openForModification());
        streamingChunk->appendData("chunkData");
        streamingChunk->doneModification(StreamingChunk::rcEndOfData);

        m_readyChunk = streamingChunk;
    }

    std::unique_ptr<AbstractStreamingChunkProvider> createStreamingChunkProvider()
    {
        return std::make_unique<StreamingChunkProvider>();
    }

    void readBytesFromChunk(StreamingChunkPtr streamingChunk, std::size_t bytesToRead)
    {
        StreamingChunkInputStream inputStream(streamingChunk.get());
        nx::Buffer readBuffer;
        ASSERT_TRUE(inputStream.tryRead(&readBuffer, bytesToRead));
        ASSERT_EQ(bytesToRead, (std::size_t)readBuffer.size());
    }
};

TEST_F(StreamingChunkCache, chunked_is_cached)
{
    givenReadyChunk();
    thenSameChunkIsReturnedByCache();
}

TEST_F(StreamingChunkCache, too_large_chunk_is_not_cached)
{
    givenReadyChunkLargerThanCache();

    thenNewChunkIsReturnedByCache();
    thenOriginalChunkHasBeenRemovedFromCache();
}

TEST_F(StreamingChunkCache, truncated_from_the_beginning_chunk_is_removed_from_cache)
{
    givenReadyChunk();

    whenChunkHasBeenTruncatedFromTheBeginning();

    thenNewChunkIsReturnedByCache();
    thenOriginalChunkHasBeenRemovedFromCache();
}

} // namespace test
