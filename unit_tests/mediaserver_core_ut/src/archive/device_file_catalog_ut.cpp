#include <gtest/gtest.h>

#include <recorder/device_file_catalog.h>
#include <recording/time_period_list.h>

namespace nx {
namespace test {

TEST(DeviceFileCatalog, main)
{
    DeviceFileCatalog catalog(QString(), QnServer::HiQualityCatalog, QnServer::StoragePool::Normal);

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
            1000000); //< Unlimited result size.
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

} // test
} // nx
