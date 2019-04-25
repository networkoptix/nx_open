#include "recorder/chunks_deque.h"
#include <nx/utils/random.h>

#include <gtest/gtest.h>

namespace nx::vms::server::test {

class ChunksDeque: public ::testing::Test
{
protected:
    void givenChunksDequeWithSomeChunks()
    {
        addChunks(0, kChunksToAddCount, kStorageCount, m_deque, m_referenceDeque);
        thenAllChunksShouldBeFound();
        thenCorrectArchiveOccupiedSpaceShouldBeReported();
    }

    void given2SortedDeques()
    {
        addChunks(0, kChunksToAddCount, kStorageCount, m_deque, m_referenceDeque);
        addChunks(10, kChunksToAddCount, kStorageCount, m_deque2, m_referenceDeque2);
    }

    void whenSomeChunksRemoved()
    {
        removeChunks(10, kStorageCount);
    }

    void thenAllChunksShouldBeFound()
    {
        ASSERT_EQ(m_deque.size(), m_referenceDeque.size());
        for (size_t i = 0; i < m_deque.size(); ++i)
            ASSERT_EQ(m_referenceDeque[i].startTimeMs, m_deque[i].chunk().startTimeMs);
    }

    void thenCorrectArchiveOccupiedSpaceShouldBeReported()
    {
        assertSpace(m_deque, m_referenceDeque, 3);
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
        assertIteratorTraversal(
            [this]() { return m_deque.rbegin(); },
            [this]() { return m_deque.rend(); },
            [this]() { return m_referenceDeque.rbegin(); },
            [this]() { return m_referenceDeque.rend(); });
    }

    void assertConstIteratorsTraversal()
    {
        assertIteratorTraversal(
            [this]() { return m_deque.cbegin(); },
            [this]() { return m_deque.cend(); },
            [this]() { return m_referenceDeque.cbegin(); },
            [this]() { return m_referenceDeque.cend(); });
    }

    void assertForwardIteratorsTraversal()
    {
        assertIteratorTraversal(
            [this]() { return m_deque.begin(); },
            [this]() { return m_deque.end(); },
            [this]() { return m_referenceDeque.begin(); },
            [this]() { return m_referenceDeque.end(); });
    }

    void assertMergeWorksCorrectly()
    {
        assertMergeOperationWorksCorrectly(
            [](auto begin1, auto end1, auto begin2, auto end2, auto begin3)
            {
                return std::merge(begin1, end1, begin2, end2, begin3);
            });
    }

    void assertSetUnionWorksCorrectly()
    {
        assertMergeOperationWorksCorrectly(
            [](auto begin1, auto end1, auto begin2, auto end2, auto begin3)
            {
                return std::set_union(begin1, end1, begin2, end2, begin3);
            });
    }

    void assertRemoveIfWorksCorrectly()
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
        assertSpace(m_deque, m_referenceDeque, kStorageCount);
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
    const int kStorageCount = 3;
    const int kChunksToAddCount = 30;

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

    void assertSpace(
        server::ChunksDeque& chunksDeque, std::deque<ChunkCore>& referenceDeque, int storageCount)
    {
        for (int i = 0; i < storageCount; ++i)
        ASSERT_EQ(referenceSpace(referenceDeque, i), chunksDeque.occupiedSpace(i));
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
                constexpr bool isProxyChunk =
                    std::is_same_v<std::decay_t<decltype(chunk)>, server::ChunksDeque::ProxyChunk>;
                if constexpr (isProxyChunk)
                {
                    return
                        chunk.chunk().startTimeMs == startTimeMs
                        && chunk.chunk().storageIndex == storageIndex;
                }
                else
                {
                    return chunk.startTimeMs == startTimeMs && chunk.storageIndex == storageIndex;
                }
            });

        if (it != deque.end())
            deque.erase(it);
    }

    int referenceSpace(std::deque<ChunkCore>& referenceDeque, int storageIndex) const
    {
        return std::accumulate(
            referenceDeque.cbegin(), referenceDeque.cend(), 0,
            [storageIndex](int sum, const ChunkCore& chunk)
            {
                if (chunk.storageIndex == storageIndex)
                    return sum + chunk.fileSize;
                return sum;
            });
    }

    template<typename F>
    void assertMergeOperationWorksCorrectly(F mergeAlgorithm)
    {
        server::ChunksDeque result;
        std::deque<ChunkCore> referenceResult;
        mergeAlgorithm(
            m_deque.begin(), m_deque.end(), m_deque2.begin(), m_deque2.end(),
            server::ChunksDequeBackInserter(result));
        mergeAlgorithm(
            m_referenceDeque.begin(), m_referenceDeque.end(), m_referenceDeque2.begin(),
            m_referenceDeque2.end(),std::back_inserter(referenceResult));
        assertEquality(result, referenceResult);
        assertSpace(result, referenceResult, kStorageCount);

        const auto resultSize = result.size();
        ASSERT_NE(0, resultSize);
        result.clear();
        referenceResult.clear();
        result.resize(resultSize);
        referenceResult.resize(resultSize);

        mergeAlgorithm(
            m_deque.begin(), m_deque.end(), m_deque2.begin(), m_deque2.end(),
            result.begin());
        mergeAlgorithm(
            m_referenceDeque.begin(), m_referenceDeque.end(), m_referenceDeque2.begin(),
            m_referenceDeque2.end(), referenceResult.begin());
        assertEquality(result, referenceResult);
        assertSpace(result, referenceResult, kStorageCount);
    }

    void assertIteratorTraversal(
        auto dequeBeginF, auto dequeEndF, auto referenceDequeBeginF, auto referenceDequeEndF)
    {
        auto it1 = dequeBeginF();
        auto it2 = referenceDequeBeginF();
        for (; it1 != dequeEndF(); ++it1)
        {
            ASSERT_EQ(it2->startTimeMs, it1->startTimeMs);
            ASSERT_EQ(it2->fileSize, it1->getFileSize());
            ASSERT_EQ(it2->storageIndex, it1->storageIndex);
            ASSERT_EQ(it2->startTimeMs, (*it1).chunk().startTimeMs);
            ASSERT_EQ(it2->fileSize, (*it1).chunk().getFileSize());
            ASSERT_EQ(it2->storageIndex, (*it1).chunk().storageIndex);
            ++it2;
        }

        ASSERT_EQ(dequeEndF(), it1);
        ASSERT_EQ(referenceDequeEndF(), it2);

        it1 = dequeBeginF();
        it2 = referenceDequeBeginF();
        for (; it1 != dequeEndF(); it1++)
        {
            ASSERT_EQ(it2->startTimeMs, it1->startTimeMs);
            ASSERT_EQ(it2->fileSize, it1->getFileSize());
            ASSERT_EQ(it2->storageIndex, it1->storageIndex);
            ASSERT_EQ(it2->startTimeMs, (*it1).chunk().startTimeMs);
            ASSERT_EQ(it2->fileSize, (*it1).chunk().getFileSize());
            ASSERT_EQ(it2->storageIndex, (*it1).chunk().storageIndex);
            it2++;
        }

        ASSERT_EQ(dequeEndF(), it1);
        ASSERT_EQ(referenceDequeEndF(), it2);
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

TEST_F(ChunksDeque, iterators)
{
    givenChunksDequeWithSomeChunks();
    assertForwardIteratorsTraversal();
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
    assertRemoveIfWorksCorrectly();
}

TEST_F(ChunksDeque, setUnion)
{
    given2SortedDeques();
    assertSetUnionWorksCorrectly();
}

}
