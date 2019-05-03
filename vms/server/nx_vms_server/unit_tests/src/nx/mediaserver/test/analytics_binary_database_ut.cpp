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
      }

      void createTestData()
      {
          nx::vms::server::metadata::AnalyticsArchive archive(
              serverModule().metadataDatabaseDir(),
              m_camera->getPhysicalId());

          QList<QRectF> rects;
          rects << QRectF(0.0, 0.0, 0.5, 0.5);

          auto baseDateMs =
              QDateTime::fromString("2017-12-31T00:23:50", Qt::ISODate).toMSecsSinceEpoch();
          for (qint64 i = 0; i < kSteps; ++i)
          {
              qint64 valueMs = baseDateMs + i * kDeltaMs;
              m_numericDates.push_back(valueMs);
              archive.saveToArchive(
                  std::chrono::milliseconds(valueMs),
                  rects,
                  kObjectTypeId,/* bjectType*/
                  kAllAttributesHash /*allAttributesHash*/);
          }
      }

      QnChunksRequestData createRequest(Qt::SortOrder sortOrder) const
      {
          QnChunksRequestData request;
          request.resList << m_camera;
          QList<QRegion> rect;
          rect << QRegion(0, 0, Qn::kMotionGridWidth, Qn::kMotionGridHeight);
          request.filter = QJson::serialized(rect);
          request.periodsType = Qn::AnalyticsContent;
          request.sortOrder = sortOrder;
          return request;
      }

      void checkData(const QnTimePeriodList& periods, int expectedRecords)
      {
          ASSERT_EQ(expectedRecords, periods.size());
          for (int i = 0; i < periods.size(); ++i)
              ASSERT_EQ(m_numericDates[i], periods[i].startTimeMs);
      }

      QnVirtualCameraResourcePtr m_camera;
      std::unique_ptr<DataProviderStub> m_dataProviderStub;
      std::vector<qint64> m_numericDates;
};

TEST_F(AnalyticsArchive, findAnalyticsAsc)
{
    createTestData();

    nx::vms::server::metadata::AnalyticsHelper archive(
        serverModule().metadataDatabaseDir());

    QnChunksRequestData request = createRequest(Qt::SortOrder::AscendingOrder);

    checkData(archive.matchImage(request), m_numericDates.size());

    request.limit = 50;
    checkData(archive.matchImage(request), request.limit);

    request.detailLevel = std::chrono::milliseconds(kDeltaMs);
    checkData(archive.matchImage(request), 1);
}

TEST_F(AnalyticsArchive, findAnalyticsDesc)
{
    createTestData();

    nx::vms::server::metadata::AnalyticsHelper archive(
        serverModule().metadataDatabaseDir());

    QnChunksRequestData request = createRequest(Qt::SortOrder::DescendingOrder);
    std::reverse(m_numericDates.begin(), m_numericDates.end());

    checkData(archive.matchImage(request), m_numericDates.size());

    request.limit = 50;
    checkData(archive.matchImage(request), request.limit);

    request.detailLevel = std::chrono::milliseconds(kDeltaMs);

    auto result = archive.matchImage(request);
    ASSERT_EQ(1, result.size());
    ASSERT_EQ(m_numericDates[m_numericDates.size()-1], result[0].startTimeMs);
    auto deltaMs = m_numericDates[0] - m_numericDates[m_numericDates.size() - 1]
        + QnMotionArchive::kMinimalDurationMs;
    ASSERT_EQ(deltaMs,result[0].durationMs);
}

TEST_F(AnalyticsArchive, findAnalyticsWithFilter)
{
    createTestData();

    nx::vms::server::metadata::AnalyticsHelper archive(
        serverModule().metadataDatabaseDir());

    QnChunksRequestData request = createRequest(Qt::SortOrder::AscendingOrder);
    request.limit = 50;

    QList<QRegion> rectFilter;
    rectFilter << QRect(0, 0, 1, 1);
    request.filter = QJson::serialized<QList<QRegion>>(rectFilter);

    checkData(archive.matchImage(request), request.limit);
    request.sortOrder = Qt::SortOrder::DescendingOrder;
    std::reverse(m_numericDates.begin(), m_numericDates.end());
    checkData(archive.matchImage(request), request.limit);

    request.sortOrder = Qt::SortOrder::AscendingOrder;
    rectFilter.clear();
    rectFilter << QRect(Qn::kMotionGridWidth/2, Qn::kMotionGridHeight/2, 1, 1);
    request.filter = QJson::serialized<QList<QRegion>>(rectFilter);

    checkData(archive.matchImage(request), 0);
    request.sortOrder = Qt::SortOrder::DescendingOrder;
    checkData(archive.matchImage(request), 0);
}

} // nx::vms::server::test
