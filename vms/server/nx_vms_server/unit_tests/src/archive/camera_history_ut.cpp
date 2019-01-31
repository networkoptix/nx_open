#define GTEST_HAS_TR1_TUPLE     0
#define GTEST_USE_OWN_TR1_TUPLE 1

#define GTEST_HAS_POSIX_RE 0
#include <gtest/gtest.h>

#include <rest/handlers/camera_history_rest_handler.h>
#include <nx_ec/data/api_fwd.h>
#include <nx/vms/api/data/camera_history_data.h>
#include <media_server/media_server_module.h>

struct BuildHistoryDataAccess
{
    static nx::vms::api::CameraHistoryItemDataList buildHistoryData(
        QnCameraHistoryRestHandler* handler,
        const MultiServerPeriodDataList& chunkPeriods)
    {
        return handler->buildHistoryData(chunkPeriods);
    }
};

TEST(CameraHistory, BuildHistoryData)
{
    QnMediaServerModule serverModule;
    QnCameraHistoryRestHandler handler(&serverModule);
    {
        // s1: |---------| |-------------| |--------|
        MultiServerPeriodDataList chunkPeriods;

        MultiServerPeriodData server1Data;
        server1Data.guid = QnUuid::createUuid();
        server1Data.periods.push_back(QnTimePeriod(10, 10));
        server1Data.periods.push_back(QnTimePeriod(30, 10));
        server1Data.periods.push_back(QnTimePeriod(50, 10));

        chunkPeriods.push_back(server1Data);

        auto result = BuildHistoryDataAccess::buildHistoryData(&handler, chunkPeriods);
        ASSERT_EQ(result.size(), 1);
        ASSERT_EQ(result[0].serverGuid, server1Data.guid);
        ASSERT_EQ(result[0].timestampMs, 10);
    }

    {
        // s1:                 |----|
        // s2: |---------| |-------------| |--------|
        MultiServerPeriodDataList chunkPeriods;
        MultiServerPeriodData server1Data;
        server1Data.guid = QnUuid::createUuid();
        server1Data.periods.push_back(QnTimePeriod(1470885319635, 963520));

        MultiServerPeriodData server2Data;
        server2Data.guid = QnUuid::createUuid();
        server2Data.periods.push_back(QnTimePeriod(1470709151655, 171992961));
        server2Data.periods.push_back(QnTimePeriod(1470885319122, 343823623));
        server2Data.periods.push_back(QnTimePeriod(1471229169513, 5485460));

        chunkPeriods.push_back(server1Data);
        chunkPeriods.push_back(server2Data);

        auto result = BuildHistoryDataAccess::buildHistoryData(&handler, chunkPeriods);
        ASSERT_EQ(result.size(), 1);
        ASSERT_EQ(result[0].serverGuid, server2Data.guid);
        ASSERT_EQ(result[0].timestampMs, 1470709151655);
    }

    {
        // s1:                 |----|      |--------- this chunk is being recorded
        // s2: |---------| |-------------| |--------|
        MultiServerPeriodDataList chunkPeriods;
        MultiServerPeriodData server1Data;
        server1Data.guid = QnUuid::createUuid();
        server1Data.periods.push_back(QnTimePeriod(1470885319635, 963520));
        server1Data.periods.push_back(QnTimePeriod(1471229169513, -1));

        MultiServerPeriodData server2Data;
        server2Data.guid = QnUuid::createUuid();
        server2Data.periods.push_back(QnTimePeriod(1470709151655, 171992961));
        server2Data.periods.push_back(QnTimePeriod(1470885319122, 343823623));
        server2Data.periods.push_back(QnTimePeriod(1471229169513, 5485460));

        chunkPeriods.push_back(server1Data);
        chunkPeriods.push_back(server2Data);

        auto result = BuildHistoryDataAccess::buildHistoryData(&handler, chunkPeriods);
        ASSERT_EQ(result.size(), 2);
        ASSERT_EQ(result[0].serverGuid, server2Data.guid);
        ASSERT_EQ(result[0].timestampMs, 1470709151655);
        ASSERT_EQ(result[1].serverGuid, server1Data.guid);
        ASSERT_EQ(result[1].timestampMs, 1471229169513);
    }

    {
        // s1:                 |----|
        // s2: |---------| |-------------|
        MultiServerPeriodDataList chunkPeriods;
        MultiServerPeriodData server1Data;
        server1Data.guid = QnUuid::createUuid();
        server1Data.periods.push_back(QnTimePeriod(1470885319635, 963520));

        MultiServerPeriodData server2Data;
        server2Data.guid = QnUuid::createUuid();
        server2Data.periods.push_back(QnTimePeriod(1470709151655, 171992961));
        server2Data.periods.push_back(QnTimePeriod(1470885319122, 343823623));

        chunkPeriods.push_back(server1Data);
        chunkPeriods.push_back(server2Data);

        auto result = BuildHistoryDataAccess::buildHistoryData(&handler, chunkPeriods);
        ASSERT_EQ(result.size(), 1);
        ASSERT_EQ(result[0].serverGuid, server2Data.guid);
        ASSERT_EQ(result[0].timestampMs, 1470709151655);
    }

    {
        // s1:                 |----|        |-----|
        // s2: |---------| |-------------|       |--------|
        // s3:      |--------|
        MultiServerPeriodDataList chunkPeriods;
        MultiServerPeriodData server1Data;
        server1Data.guid = QnUuid::createUuid();
        server1Data.periods.push_back(QnTimePeriod(50, 10));
        server1Data.periods.push_back(QnTimePeriod(90, 10));

        MultiServerPeriodData server2Data;
        server2Data.guid = QnUuid::createUuid();
        server2Data.periods.push_back(QnTimePeriod(10, 20));
        server2Data.periods.push_back(QnTimePeriod(35, 25));
        server2Data.periods.push_back(QnTimePeriod(95, 15));

        MultiServerPeriodData server3Data;
        server3Data.guid = QnUuid::createUuid();
        server3Data.periods.push_back(QnTimePeriod(20, 30));

        chunkPeriods.push_back(server1Data);
        chunkPeriods.push_back(server2Data);
        chunkPeriods.push_back(server3Data);

        auto result = BuildHistoryDataAccess::buildHistoryData(&handler, chunkPeriods);
        ASSERT_EQ(result.size(), 5);
        ASSERT_EQ(result[0].serverGuid, server2Data.guid);
        ASSERT_EQ(result[1].serverGuid, server3Data.guid);
        ASSERT_EQ(result[2].serverGuid, server2Data.guid);
        ASSERT_EQ(result[3].serverGuid, server1Data.guid);
        ASSERT_EQ(result[4].serverGuid, server2Data.guid);

        ASSERT_EQ(result[0].timestampMs, 10);
        ASSERT_EQ(result[1].timestampMs, 20);
        ASSERT_EQ(result[2].timestampMs, 35);
        ASSERT_EQ(result[3].timestampMs, 90);
        ASSERT_EQ(result[4].timestampMs, 95);
    }
}