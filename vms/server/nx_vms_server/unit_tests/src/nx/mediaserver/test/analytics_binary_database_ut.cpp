#include <gtest/gtest.h>

#include <media_server_module_fixture.h>
#include <media_server/media_server_module.h>
#include <motion/motion_helper.h>
#include <nx/streaming/abstract_stream_data_provider.h>
#include "../camera_mock.h"
#include <nx/fusion/model_functions.h>
#include <nx/vms/server/metadata/analytics_archive.h>
#include <nx/vms/server/metadata/analytics_helper.h>

namespace nx::vms::server::test
{

static const int kSteps = 1200;
static const int kDeltaMs = 10 * 1000LL;
static const int kObjectTypeId = 1;
static const int kAllAttributesHash = 1;

auto kbaseDateMs =
    QDateTime::fromString("2017-12-31T00:23:50", Qt::ISODate).toMSecsSinceEpoch();


using namespace std::chrono;
using namespace nx::vms::server::metadata;

class DataProviderStub: public QnAbstractStreamDataProvider
{
public:
    using QnAbstractStreamDataProvider::QnAbstractStreamDataProvider;
};

class AnalyticsArchive: public MediaServerModuleFixture
{
  public:
      AnalyticsArchive(): MediaServerModuleFixture()
      {
          m_camera.reset(new resource::test::CameraMock(&serverModule()));
          m_camera->setPhysicalId("analytics binary archive test");
          m_dataProviderStub.reset(new DataProviderStub(m_camera));

          m_resList << m_camera;
      }

      void createTestData()
      {
          nx::vms::server::metadata::AnalyticsArchive archive(
              serverModule().metadataDatabaseDir(),
              m_camera->getPhysicalId());

          std::vector<QRectF> rects{QRectF(0.0, 0.0, 0.5, 0.5)};

          for (qint64 i = 0; i < kSteps; ++i)
          {
              qint64 valueMs = kbaseDateMs + i * kDeltaMs;
              m_numericDates.push_back(valueMs);
              archive.saveToArchive(
                  std::chrono::milliseconds(valueMs),
                  rects,
                  i + 1,
                  kObjectTypeId,/* bjectType*/
                  kAllAttributesHash /*allAttributesHash*/);
          }
      }

      metadata::AnalyticsArchive::AnalyticsFilter createRequest(Qt::SortOrder sortOrder) const
      {
          metadata::AnalyticsArchive::AnalyticsFilter request;
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

      QnVirtualCameraResourcePtr m_camera;
      QnSecurityCamResourceList m_resList;
      std::unique_ptr<DataProviderStub> m_dataProviderStub;
      std::vector<qint64> m_numericDates;
};

TEST_F(AnalyticsArchive, findAnalyticsAsc)
{
    createTestData();

    nx::vms::server::metadata::AnalyticsHelper archive(
        serverModule().metadataDatabaseDir());

    auto request = createRequest(Qt::SortOrder::AscendingOrder);

    checkData(archive.matchImage(m_resList, request), m_numericDates.size());

    request.limit = 50;
    checkData(archive.matchImage(m_resList, request), request.limit);

    request.detailLevel = std::chrono::milliseconds(kDeltaMs);
    checkData(archive.matchImage(m_resList, request), 1);
}

TEST_F(AnalyticsArchive, findAnalyticsDesc)
{
    createTestData();

    nx::vms::server::metadata::AnalyticsHelper archive(
        serverModule().metadataDatabaseDir());

    auto request = createRequest(Qt::SortOrder::DescendingOrder);
    std::reverse(m_numericDates.begin(), m_numericDates.end());

    auto response = archive.matchImage(m_resList, request);
    checkData(response, m_numericDates.size());

    request.limit = 50;
    checkData(archive.matchImage(m_resList, request), request.limit);

    request.detailLevel = std::chrono::milliseconds(kDeltaMs);

    auto result = archive.matchImage(m_resList, request);
    ASSERT_EQ(1, result.size());
    ASSERT_EQ(m_numericDates[m_numericDates.size()-1], result[0].startTimeMs);
    static const std::chrono::milliseconds kAggregationInterval(
        nx::vms::server::metadata::AnalyticsArchive::kAggregationInterval);

    auto deltaMs = m_numericDates[0] - m_numericDates[m_numericDates.size() - 1]
        + kAggregationInterval.count();
    ASSERT_EQ(deltaMs,result[0].durationMs);
}

TEST_F(AnalyticsArchive, findAnalyticsWithFilter)
{
    createTestData();

    nx::vms::server::metadata::AnalyticsHelper archive(
        serverModule().metadataDatabaseDir());

    auto request = createRequest(Qt::SortOrder::AscendingOrder);
    QRegion rectFilter = QRect(0, 0, 1, 1);
    request.region = rectFilter;

    request.sortOrder = Qt::SortOrder::DescendingOrder;
    request.limit = 5;
    request.startTime = std::chrono::milliseconds(m_numericDates[4]);
    request.endTime = std::chrono::milliseconds(m_numericDates[5]);
    auto result = archive.matchImage(m_resList, request);
    ASSERT_EQ(2, result.size());
    ASSERT_EQ(result[0].startTimeMs, m_numericDates[5]);
    ASSERT_EQ(result[1].startTimeMs, m_numericDates[4]);

    request.limit = 50;
    request.startTime = 0ms;
    request.endTime = std::chrono::milliseconds(DATETIME_NOW);
    request.sortOrder = Qt::SortOrder::AscendingOrder;
    checkData(archive.matchImage(m_resList, request), request.limit);

    request.sortOrder = Qt::SortOrder::DescendingOrder;
    std::reverse(m_numericDates.begin(), m_numericDates.end());
    checkData(archive.matchImage(m_resList, request), request.limit);

    request.sortOrder = Qt::SortOrder::AscendingOrder;
    rectFilter = QRect(Qn::kMotionGridWidth/2, Qn::kMotionGridHeight/2, 1, 1);
    request.region = rectFilter;

    checkData(archive.matchImage(m_resList, request), 0);
    request.sortOrder = Qt::SortOrder::DescendingOrder;
    checkData(archive.matchImage(m_resList, request), 0);
}

TEST_F(AnalyticsArchive, matchObjectGroups)
{
    static const std::chrono::milliseconds kAggregationInterval(
        nx::vms::server::metadata::AnalyticsArchive::kAggregationInterval);

    createTestData();

    metadata::AnalyticsArchive archive(serverModule().metadataDatabaseDir(), m_camera->getPhysicalId());
    auto request = createRequest(Qt::SortOrder::AscendingOrder);
    request.limit = 100;

#if 0
    auto result = archive.matchObjects(request);
    auto values = std::get<0>(result);
    auto timePeriod = std::get<1>(result);
    ASSERT_EQ(request.limit, values.size());
    ASSERT_EQ(1, values[0]);
    ASSERT_EQ(request.limit, values[values.size()-1]);
    ASSERT_EQ(kbaseDateMs, timePeriod.startTimeMs);
    ASSERT_EQ((request.limit-1) * kDeltaMs + kAggregationInterval.count(), timePeriod.durationMs);
#endif

    request.sortOrder = Qt::SortOrder::DescendingOrder;
    request.limit = 100;
    auto result = archive.matchObjects(request);
    auto values = std::get<0>(result);
    auto timePeriod = std::get<1>(result);
    ASSERT_EQ(request.limit, values.size());
    ASSERT_EQ(kSteps, values[0]);
    ASSERT_EQ(kSteps - request.limit + 1, values[values.size() - 1]);
}

} // nx::vms::server::test
