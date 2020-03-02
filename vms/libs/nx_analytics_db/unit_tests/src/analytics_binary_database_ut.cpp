#include <gtest/gtest.h>

#include <nx/streaming/abstract_stream_data_provider.h>
#include <nx/fusion/model_functions.h>
#include <nx/analytics/db/analytics_archive.h>

#include <nx/utils/test_support/test_with_temporary_directory.h>

namespace nx::analytics::db::test {

static const int kSteps = 1200;
static const int kDeltaMs = 10 * 1000LL;
static const int kObjectTypeId = 1;
static const int kAllAttributesHash = 1;

auto kBaseDateMs =
    QDateTime::fromString("2017-12-31T00:23:50", Qt::ISODate).toMSecsSinceEpoch();

auto kBaseDateMs2 =
    QDateTime::fromString("2018-01-31T00:23:50", Qt::ISODate).toMSecsSinceEpoch();

using namespace std::chrono;
using namespace nx::vms::metadata;

class AnalyticsDbArchive:
    public ::testing::Test,
    public nx::utils::test::TestWithTemporaryDirectory
{
public:
    AnalyticsDbArchive():
        nx::utils::test::TestWithTemporaryDirectory("analytics_db_archive")
    {
    }

    void createTestData()
    {
        AnalyticsArchive archive(
            metadataDatabaseDir(),
            cameraPhysicalId());

        std::vector<QRectF> rects{ QRectF(0.0, 0.0, 0.5, 0.5) };

        for (qint64 i = 0; i < kSteps; ++i)
        {
            qint64 valueMs = kBaseDateMs + i * kDeltaMs;
            m_numericDates.push_back(valueMs);
            archive.saveToArchive(
                std::chrono::milliseconds(valueMs),
                rects,
                i + 1,
                kObjectTypeId,/* bjectType*/
                kAllAttributesHash /*allAttributesHash*/);
        }
    }

    AnalyticsArchive::AnalyticsFilter createRequest(Qt::SortOrder sortOrder) const
    {
        AnalyticsArchive::AnalyticsFilter request;
        request.region = QRegion(0, 0, Qn::kMotionGridWidth, Qn::kMotionGridHeight);
        request.sortOrder = sortOrder;
        request.objectTypes.push_back(kObjectTypeId);
        request.allAttributesHash.push_back(kAllAttributesHash);
        request.endTime = std::chrono::milliseconds(std::numeric_limits<int64_t>::max());
        return request;
    }

    void checkData(const QnTimePeriodList& periods, int expectedRecords)
    {
        ASSERT_EQ(expectedRecords, periods.size());
        for (int i = 0; i < periods.size(); ++i)
            ASSERT_EQ(m_numericDates[i], periods[i].startTimeMs);
    }

    QString metadataDatabaseDir() const
    {
        //return serverModule().metadataDatabaseDir();
        return testDataDir() + "/analytics_archive";
    }

    QString cameraPhysicalId() const
    {
        //return m_camera->getPhysicalId();
        return "analytics binary archive test";
    }

    QnTimePeriodList matchImage(
        AnalyticsArchive::AnalyticsFilter filter) const
    {
        AnalyticsArchive archive(
            metadataDatabaseDir(),
            cameraPhysicalId());

        return archive.matchPeriod(filter);
    }

    AnalyticsArchive::MatchObjectsResult matchObjects(
        AnalyticsArchive::AnalyticsFilter filter) const
    {
        AnalyticsArchive archive(
            metadataDatabaseDir(),
            cameraPhysicalId());

        return archive.matchObjects(filter);
    }

    //QnVirtualCameraResourcePtr m_camera;
    //QnSecurityCamResourceList m_resList;
    //std::unique_ptr<QnAbstractStreamDataProvider> m_dataProviderStub;
    std::vector<qint64> m_numericDates;
};

TEST_F(AnalyticsDbArchive, findAnalyticsAsc)
{
    createTestData();

    auto request = createRequest(Qt::SortOrder::AscendingOrder);

    checkData(matchImage(request), (int) m_numericDates.size());

    request.limit = 50;
    checkData(matchImage(request), request.limit);

    request.detailLevel = std::chrono::milliseconds(kDeltaMs);
    checkData(matchImage(request), 1);
}

TEST_F(AnalyticsDbArchive, findAnalyticsDesc)
{
    createTestData();

    auto request = createRequest(Qt::SortOrder::DescendingOrder);
    std::reverse(m_numericDates.begin(), m_numericDates.end());

    auto response = matchImage(request);
    checkData(response, (int) m_numericDates.size());

    request.limit = 50;
    checkData(matchImage(request), request.limit);

    request.detailLevel = std::chrono::milliseconds(kDeltaMs);

    auto result = matchImage(request);
    ASSERT_EQ(1, result.size());
    ASSERT_EQ(m_numericDates[m_numericDates.size()-1], result[0].startTimeMs);
    static const std::chrono::milliseconds kAggregationInterval(
        AnalyticsArchive::kAggregationInterval);

    auto deltaMs = m_numericDates[0] - m_numericDates[m_numericDates.size() - 1]
        + kAggregationInterval.count();
    ASSERT_EQ(deltaMs,result[0].durationMs);
}

TEST_F(AnalyticsDbArchive, findAnalyticsWithFilter)
{
    createTestData();

    auto request = createRequest(Qt::SortOrder::AscendingOrder);
    QRegion rectFilter = QRect(0, 0, 1, 1);
    request.region = rectFilter;

    request.sortOrder = Qt::SortOrder::DescendingOrder;
    request.limit = 5;
    request.startTime = std::chrono::milliseconds(m_numericDates[4]);
    request.endTime = std::chrono::milliseconds(m_numericDates[5]);
    auto result = matchImage(request);
    ASSERT_EQ(2, result.size());
    ASSERT_EQ(result[0].startTimeMs, m_numericDates[5]);
    ASSERT_EQ(result[1].startTimeMs, m_numericDates[4]);

    request.limit = 50;
    request.startTime = 0ms;
    request.endTime = std::chrono::milliseconds(DATETIME_NOW);
    request.sortOrder = Qt::SortOrder::AscendingOrder;
    checkData(matchImage(request), request.limit);

    request.sortOrder = Qt::SortOrder::DescendingOrder;
    std::reverse(m_numericDates.begin(), m_numericDates.end());
    checkData(matchImage(request), request.limit);

    request.sortOrder = Qt::SortOrder::AscendingOrder;
    rectFilter = QRect(Qn::kMotionGridWidth/2, Qn::kMotionGridHeight/2, 1, 1);
    request.region = rectFilter;

    checkData(matchImage(request), 0);
    request.sortOrder = Qt::SortOrder::DescendingOrder;
    checkData(matchImage(request), 0);
}

TEST_F(AnalyticsDbArchive, matchObjectGroups)
{
    static const std::chrono::milliseconds kAggregationInterval(
        AnalyticsArchive::kAggregationInterval);

    createTestData();

    auto request = createRequest(Qt::SortOrder::AscendingOrder);
    request.limit = 100;

    auto result = matchObjects(request);
    ASSERT_EQ(request.limit, result.data.size());
    ASSERT_EQ(1, result.data[0].trackGroupId);
    ASSERT_EQ(request.limit, result.data[result.data.size()-1].trackGroupId);
    ASSERT_EQ(kBaseDateMs, result.boundingPeriod.startTimeMs);
    ASSERT_EQ((request.limit-1) * kDeltaMs + kAggregationInterval.count(),
        result.boundingPeriod.durationMs);

    request.sortOrder = Qt::SortOrder::DescendingOrder;
    request.limit = 100;
    request.endTime = milliseconds(kBaseDateMs + (kSteps - 1) * kDeltaMs - 1);
    result = matchObjects(request);
    ASSERT_EQ(request.limit, result.data.size());
    ASSERT_EQ(kSteps - 1, result.data[0].trackGroupId);
    ASSERT_EQ(kSteps - request.limit, result.data[result.data.size()-1].trackGroupId);

    request.limit = 2;
    request.sortOrder = Qt::SortOrder::DescendingOrder;
    request.endTime = milliseconds(kBaseDateMs + (kSteps - 1) * kDeltaMs);
    request.startTime = milliseconds(request.endTime.count() - kDeltaMs);
    result = matchObjects(request);
    ASSERT_EQ(2, result.data.size());
    ASSERT_EQ(kSteps, result.data[0].trackGroupId);
    ASSERT_EQ(kSteps-1, result.data[1].trackGroupId);

    request.sortOrder = Qt::SortOrder::DescendingOrder;
    request.limit = -1;
    request.startTime = milliseconds(kBaseDateMs + kDeltaMs);
    request.endTime = milliseconds(kBaseDateMs + (kSteps - 1) * kDeltaMs - 1);
    result = matchObjects(request);
    for (std::size_t i = 0; i < result.data.size(); ++i)
        ASSERT_EQ(kSteps - 1 - i, result.data[i].trackGroupId);
}

TEST_F(AnalyticsDbArchive, matchLongArchive)
{
    static const std::chrono::milliseconds kAggregationInterval(
        AnalyticsArchive::kAggregationInterval);

    const std::vector<qint64> timePointsMs = {kBaseDateMs, kBaseDateMs2, kBaseDateMs2 + kDeltaMs};
    for (auto timestampMs: timePointsMs)
    {
        AnalyticsArchive archive(
            metadataDatabaseDir(),
            cameraPhysicalId());
        std::vector<QRectF> rects{QRectF(0.0, 0.0, 0.5, 0.5)};
        archive.saveToArchive(
            std::chrono::milliseconds(timestampMs),
            rects,
            /*trackGroupId*/ 0,
            kObjectTypeId,
            kAllAttributesHash /*allAttributesHash*/);
    }

    auto result = matchObjects(createRequest(Qt::SortOrder::AscendingOrder));

    ASSERT_EQ(timePointsMs.size(), result.data.size());
    for (std::size_t i = 0; i < timePointsMs.size(); ++i)
        ASSERT_EQ(timePointsMs[i], result.data[i].timestampMs);
}

} // namespace nx::analytics::db::test
