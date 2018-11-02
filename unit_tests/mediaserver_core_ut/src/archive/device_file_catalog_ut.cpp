#include <gtest/gtest.h>

#include <recorder/device_file_catalog.h>
#include <recording/time_period_list.h>
#include <media_server/media_server_module.h>

namespace nx {
namespace test {

TEST(DeviceFileCatalog, catalogRange)
{
    QnMediaServerModule serverModule;

    DeviceFileCatalog catalog(
        &serverModule,
        QString(), QnServer::HiQualityCatalog, QnServer::StoragePool::Normal);

    DeviceFileCatalog::Chunk chunk1;
    chunk1.startTimeMs = 1400000000LL * 1000;
    chunk1.durationMs = 45 * 1000;
    catalog.addRecord(chunk1);

    DeviceFileCatalog::Chunk chunk2;
    chunk2.startTimeMs = chunk1.endTimeMs();
    chunk2.durationMs = 65 * 1000;
    catalog.addRecord(chunk2);

    DeviceFileCatalog::Chunk chunk3;
    chunk3.startTimeMs = chunk2.endTimeMs() + 10 * 1000;
    chunk3.durationMs = 55 * 1000;
    catalog.addRecord(chunk3);

    auto getChunks = [&catalog] (qint64 startTimeMs, qint64 endTimeMs)
    {
        return catalog.getTimePeriods(
            startTimeMs, endTimeMs,
            1,  //< Detail level.
            true,  //< Keep small.
            1000000,
            Qt::SortOrder::AscendingOrder); //< Unlimited result size.
    };

    {
        QnTimePeriodList allPeriods = getChunks(chunk1.startTimeMs, chunk3.endTimeMs());
        ASSERT_EQ(2, allPeriods.size());
        ASSERT_EQ(chunk1.startTimeMs, allPeriods.front().startTimeMs);
        ASSERT_EQ(chunk3.endTimeMs(), allPeriods.last().endTimeMs());
    }

    {
        QnTimePeriodList allPeriods = getChunks(chunk1.endTimeMs() - 1, chunk3.endTimeMs());
        ASSERT_EQ(2, allPeriods.size());
        ASSERT_EQ(chunk1.startTimeMs, allPeriods.front().startTimeMs);
        ASSERT_EQ(chunk3.endTimeMs(), allPeriods.last().endTimeMs());
    }

    {
        QnTimePeriodList allPeriods = getChunks(chunk1.startTimeMs, chunk3.startTimeMs);
        ASSERT_EQ(1, allPeriods.size());
        ASSERT_EQ(chunk1.startTimeMs, allPeriods.front().startTimeMs);
        ASSERT_EQ(chunk2.endTimeMs(), allPeriods.last().endTimeMs());
    }

    {
        QnTimePeriodList allPeriods = getChunks(chunk2.startTimeMs, chunk3.endTimeMs());
        ASSERT_EQ(2, allPeriods.size());
        ASSERT_EQ(chunk2.startTimeMs, allPeriods.front().startTimeMs);
        ASSERT_EQ(chunk3.endTimeMs(), allPeriods.last().endTimeMs());
    }
}

TEST(DeviceFileCatalog, mergeData)
{
    QnMediaServerModule serverModule;
    DeviceFileCatalog catalog(
        &serverModule,
        QString(), QnServer::HiQualityCatalog, QnServer::StoragePool::Normal);
    static const int kRecordsToTest = 1000;

    std::deque<DeviceFileCatalog::Chunk> chunks1;
    catalog.addChunks(chunks1);
    ASSERT_EQ(0, catalog.size());

    for (int i = 0; i < kRecordsToTest; ++i)
    {
        DeviceFileCatalog::Chunk chunk;
        chunk.startTimeMs = i * 10;
        chunk.durationMs = 10;
        chunks1.push_back(chunk);
    }
    catalog.addChunks(chunks1);
    ASSERT_EQ(kRecordsToTest, catalog.size());

    std::deque<DeviceFileCatalog::Chunk> chunks2;
    for (int i = 0; i < kRecordsToTest; ++i)
    {
        DeviceFileCatalog::Chunk chunk;
        chunk.startTimeMs = i * 10 + 5;
        chunk.durationMs = 10;
        chunks2.push_back(chunk);
    }

    catalog.addChunks(chunks2);
    ASSERT_EQ(kRecordsToTest * 2, catalog.size());

    catalog.addChunks(std::deque<DeviceFileCatalog::Chunk>());
    ASSERT_EQ(kRecordsToTest * 2, catalog.size());

    catalog.addChunks(chunks2);
    catalog.addChunks(chunks1);
    ASSERT_EQ(kRecordsToTest * 2, catalog.size());

    {
        DeviceFileCatalog::Chunk chunk;
        chunk.startTimeMs = kRecordsToTest/2 * 10 + 4;
        chunk.durationMs = 10;
        catalog.addRecord(chunk);
    }
    ASSERT_EQ(kRecordsToTest * 2 + 1, catalog.size());

    for (int i = 0; i < kRecordsToTest; ++i)
    {
        DeviceFileCatalog::Chunk chunk;
        chunk.startTimeMs = i * 10 + 3;
        chunk.durationMs = 10;
        chunks2.push_back(chunk);
    }
    std::sort(chunks2.begin(), chunks2.end());

    catalog.addChunks(chunks2);
    ASSERT_EQ(kRecordsToTest * 3 + 1, catalog.size());
}

TEST(DeviceFileCatalog, ChunksDeque_insertRemove)
{
    DeviceFileCatalog::Chunk storage1Chunk, storage2Chunk;
    DeviceFileCatalog::ChunksDeque chunksDeque;
    const int count = 5;
    const int storage1Index = 0;
    const int storage2Index = 1;

    storage1Chunk.storageIndex = storage1Index;
    storage2Chunk.storageIndex = storage2Index;

    for (int i = 0; i < count; ++i)
        chunksDeque.insert(chunksDeque.end(), storage1Chunk);

    ASSERT_TRUE(chunksDeque.hasArchive(storage1Index));
    ASSERT_FALSE(chunksDeque.hasArchive(storage2Index));

    for (int i = 0; i < count; ++i)
        chunksDeque.insert(chunksDeque.end(), storage2Chunk);

    ASSERT_TRUE(chunksDeque.hasArchive(storage1Index));
    ASSERT_TRUE(chunksDeque.hasArchive(storage2Index));


    for (int i = 0; i < count; ++i)
        chunksDeque.erase(chunksDeque.begin());

    ASSERT_FALSE(chunksDeque.hasArchive(storage1Index));
    ASSERT_TRUE(chunksDeque.hasArchive(storage2Index));

    for (int i = 0; i < count; ++i)
        chunksDeque.erase(chunksDeque.begin());

    ASSERT_FALSE(chunksDeque.hasArchive(storage1Index));
    ASSERT_FALSE(chunksDeque.hasArchive(storage2Index));
}

TEST(DeviceFileCatalog, ChunksDeque_reCalcPresence)
{
    DeviceFileCatalog::Chunk storage1Chunk, storage2Chunk;
    DeviceFileCatalog::ChunksDeque chunksDeque;
    const int count = 5;
    const int storage1Index = 0;
    const int storage2Index = 1;

    storage1Chunk.storageIndex = storage1Index;
    storage2Chunk.storageIndex = storage2Index;

    for (int i = 0; i < count; ++i)
        chunksDeque.insert(chunksDeque.end(), storage1Chunk);

    ASSERT_TRUE(chunksDeque.hasArchive(storage1Index));
    ASSERT_FALSE(chunksDeque.hasArchive(storage2Index));

    std::vector<DeviceFileCatalog::Chunk> chunksForMerge;
    chunksForMerge.push_back(storage2Chunk);
    chunksForMerge.push_back(storage2Chunk);

    int oldSize = chunksDeque.size();
    chunksDeque.resize(chunksDeque.size() + chunksForMerge.size());
    std::copy(chunksForMerge.cbegin(), chunksForMerge.cend(), chunksDeque.begin() + oldSize);
    ASSERT_FALSE(chunksDeque.hasArchive(storage2Index));

    chunksDeque.reCalcArchivePresence();
    ASSERT_TRUE(chunksDeque.hasArchive(storage2Index));
}

} // test
} // nx
