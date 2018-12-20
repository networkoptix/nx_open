#include <gtest/gtest.h>

#include <rest/handlers/multiserver_chunks_rest_handler.h>

TEST(MultiserverPeriods, mergeByCameraId)
{
    MultiServerPeriodData server1Data;
    server1Data.guid = QnUuid::createUuid();
    server1Data.periods.push_back(QnTimePeriod(10, 10));
    server1Data.periods.push_back(QnTimePeriod(30, 10));
    server1Data.periods.push_back(QnTimePeriod(50, 10));

    MultiServerPeriodData server2Data;
    server2Data.guid = QnUuid::createUuid();
    server2Data.periods.push_back(QnTimePeriod(10, 10));
    server2Data.periods.push_back(QnTimePeriod(30, 10));
    server2Data.periods.push_back(QnTimePeriod(50, 10));

    {
        MultiServerPeriodDataList chunkPeriods;
        chunkPeriods.push_back(server1Data);
        chunkPeriods.push_back(server2Data);

        auto result = QnMultiserverChunksRestHandler::mergeDataWithSameId(
            chunkPeriods, 5, Qt::SortOrder::AscendingOrder);
        ASSERT_EQ(2, result.size());
        ASSERT_EQ(3, result[0].periods.size());
        ASSERT_EQ(3, result[1].periods.size());
    }
    {
        MultiServerPeriodDataList chunkPeriods;
        chunkPeriods.push_back(server1Data);
        chunkPeriods.push_back(server1Data);

        auto result = QnMultiserverChunksRestHandler::mergeDataWithSameId(
            chunkPeriods, 5, Qt::SortOrder::AscendingOrder);
        ASSERT_EQ(1, result.size());
        ASSERT_EQ(3, result[0].periods.size());
    }

    std::reverse(server1Data.periods.begin(), server1Data.periods.end());
    std::reverse(server2Data.periods.begin(), server2Data.periods.end());

    {
        MultiServerPeriodDataList chunkPeriods;
        chunkPeriods.push_back(server1Data);
        chunkPeriods.push_back(server1Data);

        auto result = QnMultiserverChunksRestHandler::mergeDataWithSameId(
            chunkPeriods, 2, Qt::SortOrder::DescendingOrder);
        ASSERT_EQ(1, result.size());
        ASSERT_EQ(2, result[0].periods.size());
        ASSERT_EQ(50, result[0].periods[0].startTimeMs);
        ASSERT_EQ(30, result[0].periods[1].startTimeMs);
    }
}
