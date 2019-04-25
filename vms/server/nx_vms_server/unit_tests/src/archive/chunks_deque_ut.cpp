#include "recorder/chunks_deque.h"
#include <nx/utils/random.h>

#include <gtest/gtest.h>

namespace nx::vms::server::test {

class ChunksDeque: public ::testing::Test
{
protected:
    void givenChunksDequeWithSomeChunks()
    {
        addChunks(0, /*chunksCount*/30, /*storageCount*/3, m_deque, m_referenceDeque);
        thenAllChunksShouldBeFound();
        thenCorrectArchiveOccupiedSpaceShouldBeReported();
    }

    void given2SortedDeques()
    {
        addChunks(0, /*chunksCount*/30, /*storageCount*/3, m_deque, m_referenceDeque);
        addChunks(10, /*chunksCount*/30, /*storageCount*/3, m_deque2, m_referenceDeque2);
    }

    void whenSomeChunksRemoved()
    {
        removeChunks(10, 3);
    }

    void thenAllChunksShouldBeFound()
    {
        ASSERT_EQ(m_deque.size(), m_referenceDeque.size());
        for (size_t i = 0; i < m_deque.size(); ++i)
            ASSERT_EQ(m_referenceDeque[i].startTimeMs, m_deque[i].chunk().startTimeMs);
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

    void assertLowerBoundWorksCorrectly()
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

    void assertUpperBoundWorksCorrectly()
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

    void whenChunksAreReversed()
    {
        std::reverse(m_deque.begin(), m_deque.end());
        std::reverse(m_referenceDeque.begin(), m_referenceDeque.end());
        ASSERT_FALSE(std::is_sorted(m_deque.cbegin(), m_deque.cend()));
        ASSERT_FALSE(std::is_sorted(m_referenceDeque.cbegin(), m_referenceDeque.cend()));
    }

    void whenChunksAreSorted()
    {
        std::sort(m_deque.begin(), m_deque.end());
        std::sort(m_referenceDeque.begin(), m_referenceDeque.end());
    }

    void assertReverseIteratorsTraversal()
    {
        auto rit1 = m_deque.rbegin();
        auto rit2 = m_referenceDeque.rbegin();
        for (; rit1 != m_deque.rend(); ++rit1)
        {
            ASSERT_EQ(rit2->startTimeMs, rit1->startTimeMs);
            ASSERT_EQ(rit2->fileSize, rit1->getFileSize());
            ASSERT_EQ(rit2->storageIndex, rit1->storageIndex);
            ASSERT_EQ(rit2->startTimeMs, (*rit1).chunk().startTimeMs);
            ASSERT_EQ(rit2->fileSize, (*rit1).chunk().getFileSize());
            ASSERT_EQ(rit2->storageIndex, (*rit1).chunk().storageIndex);
            ++rit2;
        }

        ASSERT_EQ(m_deque.rend(), rit1);
        ASSERT_EQ(m_referenceDeque.rend(), rit2);

        rit1 = m_deque.rbegin();
        rit2 = m_referenceDeque.rbegin();
        for (; rit1 != m_deque.rend(); rit1++)
        {
            ASSERT_EQ(rit2->startTimeMs, rit1->startTimeMs);
            ASSERT_EQ(rit2->fileSize, rit1->getFileSize());
            ASSERT_EQ(rit2->storageIndex, rit1->storageIndex);
            ASSERT_EQ(rit2->startTimeMs, (*rit1).chunk().startTimeMs);
            ASSERT_EQ(rit2->fileSize, (*rit1).chunk().getFileSize());
            ASSERT_EQ(rit2->storageIndex, (*rit1).chunk().storageIndex);
            rit2++;
        }

        ASSERT_EQ(m_deque.rend(), rit1);
        ASSERT_EQ(m_referenceDeque.rend(), rit2);
    }

    void assertConstIteratorsTraversal()
    {
        auto cit1 = m_deque.cbegin();
        auto cit2 = m_referenceDeque.cbegin();
        for (; cit1 != m_deque.cend(); ++cit1)
        {
            ASSERT_EQ(cit2->startTimeMs, cit1->startTimeMs);
            ASSERT_EQ(cit2->fileSize, cit1->getFileSize());
            ASSERT_EQ(cit2->storageIndex, cit1->storageIndex);
            ASSERT_EQ(cit2->startTimeMs, (*cit1).chunk().startTimeMs);
            ASSERT_EQ(cit2->fileSize, (*cit1).chunk().getFileSize());
            ASSERT_EQ(cit2->storageIndex, (*cit1).chunk().storageIndex);
            ++cit2;
        }

        ASSERT_EQ(m_deque.cend(), cit1);
        ASSERT_EQ(m_referenceDeque.cend(), cit2);

        cit1 = m_deque.cbegin();
        cit2 = m_referenceDeque.cbegin();
        for (; cit1 != m_deque.cend(); cit1++)
        {
            ASSERT_EQ(cit2->startTimeMs, cit1->startTimeMs);
            ASSERT_EQ(cit2->fileSize, cit1->getFileSize());
            ASSERT_EQ(cit2->storageIndex, cit1->storageIndex);
            ASSERT_EQ(cit2->startTimeMs, (*cit1).chunk().startTimeMs);
            ASSERT_EQ(cit2->fileSize, (*cit1).chunk().getFileSize());
            ASSERT_EQ(cit2->storageIndex, (*cit1).chunk().storageIndex);
            cit2++;
        }

        ASSERT_EQ(m_deque.cend(), cit1);
        ASSERT_EQ(m_referenceDeque.cend(), cit2);
    }

    void assertMergeWorksCorrectly()
    {
        server::ChunksDeque result;
        std::deque<ChunkCore> referenceResult;
        std::merge(
            m_deque.begin(), m_deque.end(), m_deque2.begin(), m_deque2.end(),
            server::ChunksDequeBackInserter(result));
        std::merge(
            m_referenceDeque.begin(), m_referenceDeque.end(), m_referenceDeque2.begin(),
            m_referenceDeque2.end(), std::back_inserter(referenceResult));
        assertEquality(result, referenceResult);

        const auto resultSize = result.size();
        ASSERT_NE(0, resultSize);
        result.clear();
        referenceResult.clear();
        result.resize(resultSize);
        referenceResult.resize(resultSize);

        std::merge(
            m_deque.begin(), m_deque.end(), m_deque2.begin(), m_deque2.end(),
            result.begin());
        std::merge(
            m_referenceDeque.begin(), m_referenceDeque.end(), m_referenceDeque2.begin(),
            m_referenceDeque2.end(), referenceResult.begin());
        assertEquality(result, referenceResult);
    }

    void assertSetUnionCorrectly()
    {
        server::ChunksDeque result;
        std::deque<ChunkCore> referenceResult;
        std::set_union(
            m_deque.begin(), m_deque.end(), m_deque2.begin(), m_deque2.end(),
            server::ChunksDequeBackInserter(result));
        std::set_union(
            m_referenceDeque.begin(), m_referenceDeque.end(), m_referenceDeque2.begin(),
            m_referenceDeque2.end(),std::back_inserter(referenceResult));
        assertEquality(result, referenceResult);

        const auto resultSize = result.size();
        ASSERT_NE(0, resultSize);
        result.clear();
        referenceResult.clear();
        result.resize(resultSize);
        referenceResult.resize(resultSize);

        std::set_union(
            m_deque.begin(), m_deque.end(), m_deque2.begin(), m_deque2.end(),
            result.begin());
        std::set_union(
            m_referenceDeque.begin(), m_referenceDeque.end(), m_referenceDeque2.begin(),
            m_referenceDeque2.end(), referenceResult.begin());
        assertEquality(result, referenceResult);
    }

    void assertRemoveIfCorrectly()
    {
        std::remove_if(
            m_deque.begin(), m_deque.end(),
            [](const auto& pc)
            {
                return pc.chunk().startTimeMs % 2 == 0;
            });
        std::remove_if(
            m_referenceDeque.begin(), m_referenceDeque.end(),
            [](const auto& c)
            {
                return c.startTimeMs % 2 == 0;
            });
        assertEquality(m_deque, m_referenceDeque);
    }

private:
    struct ChunkCore
    {
        int startTimeMs = -1;
        int storageIndex = -1;
        int fileSize = -1;

        ChunkCore() = default;
        ChunkCore(int startTimeMs, int storageIndex, int fileSize):
            startTimeMs(startTimeMs), storageIndex(storageIndex), fileSize(fileSize)
        {
        }

        friend bool operator<(const ChunkCore& c1, const ChunkCore& c2)
        {
            return c1.startTimeMs < c2.startTimeMs;
        }
    };

    server::ChunksDeque m_deque;
    server::ChunksDeque m_deque2;
    std::deque<ChunkCore> m_referenceDeque;
    std::deque<ChunkCore> m_referenceDeque2;

    void addChunks(
        int64_t startTimeMs, int chunksCount, int storageCount, server::ChunksDeque& targetDeque,
        std::deque<ChunkCore>& referenceDeque)
    {
        for (int i = 0; i < storageCount; ++i)
        {
            for (int j = 0; j < chunksCount; ++j)
            {
                targetDeque.push_back(Chunk(startTimeMs + j, i, 0, 1, 0, 0, 1));
                referenceDeque.push_back(ChunkCore(startTimeMs + j, i, 1));
            }
        }
    }

    void assertEquality(server::ChunksDeque& chunksDeque, std::deque<ChunkCore>& referenceDeque)
    {
        ASSERT_TRUE(std::equal(
            chunksDeque.cbegin(), chunksDeque.cend(), referenceDeque.cbegin(), referenceDeque.cend(),
            [](const auto& pc, const auto& chunkCore)
            {
                return
                    pc.chunk().startTimeMs == chunkCore.startTimeMs
                    && pc.chunk().getFileSize() == chunkCore.fileSize
                    && pc.chunk().storageIndex == chunkCore.storageIndex;
            }));
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
    assertLowerBoundWorksCorrectly();
}

TEST_F(ChunksDeque, upperBound)
{
    givenChunksDequeWithSomeChunks();
    assertUpperBoundWorksCorrectly();
}

TEST_F(ChunksDeque, constIterators)
{
    givenChunksDequeWithSomeChunks();
    assertConstIteratorsTraversal();
}

TEST_F(ChunksDeque, reverseIterators)
{
    givenChunksDequeWithSomeChunks();
    assertReverseIteratorsTraversal();
}

TEST_F(ChunksDeque, reverseAndSort)
{
    givenChunksDequeWithSomeChunks();
    whenChunksAreReversed();
    thenAllChunksShouldBeFound();
    whenChunksAreSorted();
    thenAllChunksShouldBeFound();
}

TEST_F(ChunksDeque, merge)
{
    given2SortedDeques();
    assertMergeWorksCorrectly();
}

TEST_F(ChunksDeque, removeIf)
{
    givenChunksDequeWithSomeChunks();
    assertRemoveIfCorrectly();
}

TEST_F(ChunksDeque, setUnion)
{
    given2SortedDeques();
    assertSetUnionCorrectly();
}

}
