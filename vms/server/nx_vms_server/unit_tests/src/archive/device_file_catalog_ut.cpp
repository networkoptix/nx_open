#include "media_server_module_fixture.h"

#include <gtest/gtest.h>

#include <recorder/device_file_catalog.h>
#include <recording/time_period_list.h>
#include <recorder/chunks_deque.h>
#include <media_server/media_server_module.h>
#include <nx/utils/test_support/utils.h>

namespace nx {
namespace test {

TEST(Chunk, DefaultConstructedIsNull)
{
    nx::vms::server::Chunk chunk;
    ASSERT_TRUE(chunk.isNull());
}

TEST(DeviceFileCatalog, catalogRange)
{
    QnMediaServerModule serverModule;

    DeviceFileCatalog catalog(
        &serverModule,
        QString(), QnServer::HiQualityCatalog, QnServer::StoragePool::Normal);

    nx::vms::server::Chunk chunk1;
    chunk1.startTimeMs = 1400000000LL * 1000;
    chunk1.durationMs = 45 * 1000;
    catalog.addRecord(chunk1);

    nx::vms::server::Chunk chunk2;
    chunk2.startTimeMs = chunk1.endTimeMs();
    chunk2.durationMs = 65 * 1000;
    catalog.addRecord(chunk2);

    nx::vms::server::Chunk chunk3;
    chunk3.startTimeMs = chunk2.endTimeMs() + 10 * 1000;
    chunk3.durationMs = 55 * 1000;
    catalog.addRecord(chunk3);

    auto getChunks = [&catalog] (qint64 startTimeMs, qint64 endTimeMs, qint64 detailLevel = 1)
    {
        return catalog.getTimePeriods(
            startTimeMs, endTimeMs,
            detailLevel,
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
        QnTimePeriodList allPeriods = getChunks(chunk1.startTimeMs, chunk3.endTimeMs(), 5000000000);
        ASSERT_EQ(1, allPeriods.size());
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

    nx::vms::server::ChunksDeque chunks1;
    catalog.addChunks(chunks1);
    ASSERT_EQ(0, catalog.size());

    for (int i = 0; i < kRecordsToTest; ++i)
    {
        nx::vms::server::Chunk chunk;
        chunk.startTimeMs = i * 10;
        chunk.durationMs = 10;
        chunks1.push_back(chunk);
    }
    catalog.addChunks(chunks1);
    ASSERT_EQ(kRecordsToTest, catalog.size());

    std::deque<Chunk> chunks2;
    for (int i = 0; i < kRecordsToTest; ++i)
    {
        nx::vms::server::Chunk chunk;
        chunk.startTimeMs = i * 10 + 5;
        chunk.durationMs = 10;
        chunks2.push_back(chunk);
    }

    catalog.addChunks(chunks2);
    ASSERT_EQ(kRecordsToTest * 2, catalog.size());

    catalog.addChunks(nx::vms::server::ChunksDeque());
    ASSERT_EQ(kRecordsToTest * 2, catalog.size());

    catalog.addChunks(chunks2);
    catalog.addChunks(chunks1);
    ASSERT_EQ(kRecordsToTest * 2, catalog.size());

    {
        nx::vms::server::Chunk chunk;
        chunk.startTimeMs = kRecordsToTest/2 * 10 + 4;
        chunk.durationMs = 10;
        catalog.addRecord(chunk);
    }
    ASSERT_EQ(kRecordsToTest * 2 + 1, catalog.size());

    for (int i = 0; i < kRecordsToTest; ++i)
    {
        nx::vms::server::Chunk chunk;
        chunk.startTimeMs = i * 10 + 3;
        chunk.durationMs = 10;
        chunks2.push_back(chunk);
    }
    std::sort(chunks2.begin(), chunks2.end());

    catalog.addChunks(chunks2);
    ASSERT_EQ(kRecordsToTest * 3 + 1, catalog.size());
}

using namespace nx::vms::server;

class DeviceFileCatalogTest: public MediaServerModuleFixture
{
protected:
    static std::deque<Chunk> generateChunks(int count, int startTimeMs = 0)
    {
        std::deque<Chunk> result;
        for (int i = 0; i < count; ++i)
        {
            result.push_back(Chunk(
                /* startTimeMs */ i * 10 + startTimeMs,
                /* storageIndex */ i % 2,
                /* fileIndex */ i,
                /* duration */ i * 5000,
                /* timeZone */ 3,
                /* fileSizeHi */ 0,
                /* fileSizeLow */ i * 100));
        }

        return result;
    }

    DeviceFileCatalogPtr createCatalog(bool withChunks = false, int64_t startTimeMs = 0)
    {
        auto result = DeviceFileCatalogPtr(new ::DeviceFileCatalog(
            &serverModule(), "", QnServer::HiQualityCatalog, QnServer::StoragePool::Normal));

        if (withChunks)
        {
            const auto chunks = generateChunks(10, startTimeMs);
            result->addChunks(chunks);
            assertEquality(result, chunks);
            assertPresence(result, chunks);
        }

        return result;
    }

    static void assertPresence(const DeviceFileCatalogPtr& catalog, const std::deque<Chunk>& chunks)
    {
        ASSERT_EQ(accumDuration(chunks, 0), catalog->occupiedDuration(0));
        ASSERT_EQ(accumDuration(chunks, 1), catalog->occupiedDuration(1));
        ASSERT_EQ(accumSpace(chunks, 0), catalog->occupiedSpace(0));
        ASSERT_EQ(accumSpace(chunks, 1), catalog->occupiedSpace(1));
    }

    static void assertEquality(const DeviceFileCatalogPtr& catalog, const std::deque<Chunk>& chunks)
    {
        ASSERT_EQ(chunks.size(), catalog->size());
        ASSERT_EQ(chunks, catalog->getChunks());
        for (size_t i = 0; i < catalog->size(); ++i)
            ASSERT_EQ(chunks[i], catalog->chunkAt(i));
    }

    static std::chrono::milliseconds accumDuration(const std::deque<Chunk>& chunks, int storageIndex)
    {
        return std::chrono::milliseconds(std::accumulate(
            chunks.cbegin(), chunks.cend(), 0LL,
            [storageIndex](int64_t v, const auto& c2)
            {
                return c2.storageIndex == storageIndex ? v + c2.durationMs : v;
            }));
    }

    static int64_t accumSpace(const std::deque<Chunk>& chunks, int storageIndex)
    {
        return std::accumulate(
            chunks.cbegin(), chunks.cend(), 0LL,
            [storageIndex](int64_t v, const auto& c2)
            {
                return c2.storageIndex == storageIndex ? v + c2.getFileSize() : v;
            });
    }
};

TEST_F(DeviceFileCatalogTest, getChunks)
{
    const auto chunks = generateChunks(10);
    auto catalog = createCatalog();
    catalog->addChunks(chunks);
    ASSERT_EQ(chunks, catalog->getChunks());
    ASSERT_EQ(chunks.size(), catalog->size());
}

TEST_F(DeviceFileCatalogTest, addChunk_chunkAt)
{
    auto catalog = createCatalog(/* withChunks */ true);
    const auto newChunk = Chunk(
        catalog->getChunks()[2].startTimeMs + 1, /* storageIndex */ 0, /* fileIndex */ 1,
        /* duration */ 5, /* timeZone */ 3);

    catalog->addChunk(newChunk);
    ASSERT_EQ(11, catalog->size());
    ASSERT_EQ(newChunk.startTimeMs, catalog->chunkAt(3).startTimeMs);

    const auto catalogChunks = catalog->getChunks().toDeque();
    ASSERT_EQ(11, catalogChunks.size());
    ASSERT_EQ(newChunk.startTimeMs, catalogChunks[3].startTimeMs);

    assertPresence(catalog, catalogChunks);
    assertEquality(catalog, catalogChunks);
}

TEST_F(DeviceFileCatalogTest, takeChunks)
{
    auto catalog = createCatalog(/* withChunks */ true);
    const size_t size = catalog->size();
    const auto chunks = catalog->takeChunks();
    ASSERT_EQ(size, chunks.size());
    ASSERT_EQ(0, catalog->size());
    ASSERT_EQ(std::deque<Chunk>(), catalog->getChunks());
}

TEST_F(DeviceFileCatalogTest, removeChunks)
{
    auto catalog = createCatalog(/* withChunks */ true);
    auto chunks = catalog->getChunks().toDeque();
    chunks.erase(std::remove_if(
        chunks.begin(), chunks.end(),
        [](const auto& c) { return c.storageIndex == 0; }), chunks.end());

    catalog->removeChunks(0);
    assertEquality(catalog, chunks);
    assertPresence(catalog, chunks);
}

TEST_F(DeviceFileCatalogTest, mergeRecordingStatistics)
{
    auto catalog1 = createCatalog(/* withChunks */ true);
    auto catalog2 = createCatalog(/* withChunks */ true, /* startTimeMs */ 10000);
    const auto stats = catalog1->mergeRecordingStatisticsData(
        *catalog2,
        /* bitrateAnalyzePeriodMs */ 0,
        [](int) { return QnStorageResourcePtr(); });

    ASSERT_NE(0, stats.recordedBytes);
    ASSERT_NE(0, stats.recordedSecs);

    // #TODO #rvasilenko Elaborate on this test.
}

TEST_F(DeviceFileCatalogTest, recordedMonthList)
{
    auto catalog = createCatalog();
    QDate d1 = QDate(2000, 2, 13);
    catalog->addChunk(Chunk(
        /* startTimeMs */ QDateTime(d1).toMSecsSinceEpoch() + 100,
        /* storageIndex */ 0,
        /* fileIndex */ 0,
        /* duration */ 50000,
        /* timeZone */ 3,
        /* fileSizeHi */ 0,
        /* fileSizeLow */ 300));

    auto d2 = d1.addMonths(2);
    catalog->addChunk(Chunk(
        /* startTimeMs */ QDateTime(d2).toMSecsSinceEpoch() + 100,
        /* storageIndex */ 0,
        /* fileIndex */ 0,
        /* duration */ 50000,
        /* timeZone */ 3,
        /* fileSizeHi */ 0,
        /* fileSizeLow */ 300));

    const auto recordedMonths = catalog->recordedMonthList();
    ASSERT_EQ(2, recordedMonths.size());
    ASSERT_TRUE(std::all_of(
        recordedMonths.cbegin(), recordedMonths.cend(),
        [d1, d2](const auto& d) {return d.month() == d1.month() || d.month() == d2.month(); } ));
}

TEST_F(DeviceFileCatalogTest, setDifference)
{
    const auto chunks = generateChunks(1000);
    auto catalog1 = createCatalog();
    auto catalog2 = createCatalog();
    std::deque<Chunk> diffDeque;

    // Push every chunk to the first catalog, every other - to the second one and accumulate
    // difference in a separate container.
    for (size_t i = 0; i < chunks.size(); ++i)
    {
        catalog1->addChunk(chunks[i]);
        if (i % 2 == 0)
            catalog2->addChunk(chunks[i]);
        else
            diffDeque.push_back(chunks[i]);
    }

    ASSERT_EQ(diffDeque, catalog1->setDifference(*catalog2));
}

TEST_F(DeviceFileCatalogTest, chunksBefore)
{
    const auto chunks = generateChunks(1000);
    auto catalog = createCatalog();
    catalog->addChunks(chunks);
    const auto cutoffTimepoint = chunks[500].startTimeMs;
    std::deque<Chunk> referenceDeque;
    std::copy_if(
        chunks.cbegin(), chunks.cend(), std::back_inserter(referenceDeque),
        [cutoffTimepoint](const auto& c)
        {
            return c.startTimeMs < cutoffTimepoint && c.storageIndex == 0;
        });

    ASSERT_EQ(referenceDeque, catalog->chunksBefore(cutoffTimepoint, 0));
}

TEST_F(DeviceFileCatalogTest, chunksAfter)
{
    const auto chunks = generateChunks(1000);
    auto catalog = createCatalog();
    catalog->addChunks(chunks);
    const auto cutoffTimepoint = chunks[500].startTimeMs;
    std::deque<Chunk> referenceDeque;
    std::copy_if(
        chunks.cbegin(), chunks.cend(), std::back_inserter(referenceDeque),
        [cutoffTimepoint](const auto& c)
        {
            return c.startTimeMs > cutoffTimepoint && c.storageIndex == 0;
        });

    const auto after = catalog->chunksAfter(cutoffTimepoint, 0);
    ASSERT_EQ(referenceDeque, after);
}

TEST_F(DeviceFileCatalogTest, chunksBefore_EmptyCatalog)
{
    auto catalog = createCatalog();
    ASSERT_EQ(std::deque<Chunk>(), catalog->chunksBefore(0, 0));
}

TEST_F(DeviceFileCatalogTest, chunksBefore_wholeCatalog)
{
    const auto chunks = generateChunks(1000);
    auto catalog = createCatalog();
    catalog->addChunks(chunks);
    const auto cutoffTimepoint = chunks[chunks.size() - 1].startTimeMs;
    std::deque<Chunk> referenceDeque;
    std::copy_if(
        chunks.cbegin(), chunks.cend(), std::back_inserter(referenceDeque),
        [](const auto& c) { return c.storageIndex == 0; });

    ASSERT_EQ(referenceDeque, catalog->chunksBefore(cutoffTimepoint, 0));
}

} // test
} // nx
