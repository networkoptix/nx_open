#include "recorder/chunks_deque.h"
#include <nx/utils/random.h>

#include <gtest/gtest.h>

namespace nx::vms::server::test {

class ChunksDeque: public ::testing::Test
{
protected:
    void givenChunksDequeWithSomeChunks()
    {
        addChunks(/*chunksCount*/20, /*storageCount*/3);
        thenAllChunksShouldBeFound();
        thenCorrectArchiveOccupiedSpaceShouldBeReported();
    }

    void whenSomeChunksRemoved()
    {
        removeChunks(10, 3);
    }

    void thenAllChunksShouldBeFound()
    {
        ASSERT_EQ(m_deque.size(), m_referenceDeque.size());
    }

    void thenCorrectArchiveOccupiedSpaceShouldBeReported()
    {
        ASSERT_EQ(referenceSpace(0), m_deque.occupiedSpace(0));
        ASSERT_EQ(referenceSpace(1), m_deque.occupiedSpace(1));
    }

    void thenFindShouldReturnCorrectResult()
    {
        for (const auto& chunk: m_referenceDeque)
        {
            auto it = std::find_if(
                m_deque.begin(), m_deque.end(),
                [&chunk](const auto& proxyChunk)
                {
                    return
                        proxyChunk.chunk().startTimeMs == chunk.startTimeMs
                        && proxyChunk.chunk().storageIndex == chunk.storageIndex
                        && proxyChunk.chunk().getFileSize() == chunk.fileSize;
                });
            ASSERT_EQ(chunk.startTimeMs, (*it).chunk().startTimeMs);
            ASSERT_EQ(chunk.fileSize, (*it).chunk().getFileSize());
            ASSERT_EQ(chunk.storageIndex, (*it).chunk().storageIndex);
            ASSERT_NE(m_deque.end(), it);
            thenAllChunksShouldBeFound();
            thenCorrectArchiveOccupiedSpaceShouldBeReported();
        }
    }

    void thenLowerBoundShouldReturnCorrectResult()
    {
        for (const auto& chunk: m_referenceDeque)
        {
            auto it = std::lower_bound(m_deque.begin(), m_deque.end(), chunk.startTimeMs);
            ASSERT_EQ(chunk.startTimeMs, (*it).chunk().startTimeMs);
            ASSERT_NE(m_deque.end(), it);
            thenAllChunksShouldBeFound();
            thenCorrectArchiveOccupiedSpaceShouldBeReported();
        }
    }

private:
    struct ChunkCore
    {
        int startTimeMs;
        int storageIndex;
        int fileSize;

        ChunkCore(int startTimeMs, int storageIndex, int fileSize):
            startTimeMs(startTimeMs), storageIndex(storageIndex), fileSize(fileSize)
        {
        }
    };

    server::ChunksDeque m_deque;
    std::deque<ChunkCore> m_referenceDeque;

    void addChunks(int chunksCount, int storageCount)
    {
        for (int i = 0; i < storageCount; ++i)
        {
            for (int j = 0; j < chunksCount; ++j)
            {
                m_deque.push_back(Chunk(j, i, 0, 1, 0, 0, 1));
                m_referenceDeque.push_back(ChunkCore(j, i, 1));
            }
        }
    }

    void removeChunks(int chunksCount, int storageCount)
    {
        for (int i = 0; i < storageCount; ++i)
        {
            for (int j = 0; j < chunksCount; ++j)
            {
                int startTimeToRemove = utils::random::number<int>(0, chunksCount);
                int storageIndexToRemove = utils::random::number<int>(0, storageCount);
                removeChunk(m_deque, startTimeToRemove, storageIndexToRemove);
                removeChunk(m_referenceDeque, startTimeToRemove, storageIndexToRemove);
                ASSERT_EQ(m_deque.size(), m_referenceDeque.size());
            }
        }
    }

    template<typename D>
    void removeChunk(D& deque, int startTimeMs, int storageIndex)
    {
        auto it = std::find_if(
            deque.begin(), deque.end(),
            [startTimeMs, storageIndex](const auto& chunk)
            {
                if constexpr (std::is_same_v<std::decay_t<decltype(chunk)>, server::ChunksDeque::ProxyChunk>)
                    return chunk.chunk().startTimeMs == startTimeMs && chunk.chunk().storageIndex == storageIndex;
                else
                    return chunk.startTimeMs == startTimeMs && chunk.storageIndex == storageIndex;
            });

        if (it != deque.end())
            deque.erase(it);
    }

    int referenceSpace(int storageIndex) const
    {
        return std::accumulate(
            m_referenceDeque.cbegin(), m_referenceDeque.cend(), 0,
            [storageIndex](int sum, const ChunkCore& chunk)
            {
                if (chunk.storageIndex == storageIndex)
                    return sum + chunk.fileSize;
                return sum;
            });
    }
};

TEST_F(ChunksDeque, addRemove)
{
    givenChunksDequeWithSomeChunks();
    whenSomeChunksRemoved();
    thenAllChunksShouldBeFound();
    thenCorrectArchiveOccupiedSpaceShouldBeReported();
}

TEST_F(ChunksDeque, find)
{
    givenChunksDequeWithSomeChunks();
    thenFindShouldReturnCorrectResult();
}

TEST_F(ChunksDeque, lowerBound)
{
    givenChunksDequeWithSomeChunks();
    thenLowerBoundShouldReturnCorrectResult();
}

}
