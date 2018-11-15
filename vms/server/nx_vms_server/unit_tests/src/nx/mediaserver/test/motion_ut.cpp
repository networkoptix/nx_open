#include <gtest/gtest.h>

#include <media_server_module_fixture.h>
#include <media_server/media_server_module.h>
#include <motion/motion_helper.h>
#include <nx/streaming/abstract_stream_data_provider.h>
#include <nx/mediaserver/camera_mock.h>
#include <nx/fusion/model_functions.h>

namespace nx::mediaserver::test
{

static const int kSteps = 1200;
static const int kDeltaMs = 10 * 1000LL;

class DataProviderStub: public QnAbstractStreamDataProvider
{
public:
    using QnAbstractStreamDataProvider::QnAbstractStreamDataProvider;
};

class MotionArchive: public MediaServerModuleFixture
{
  public:
      MotionArchive(): MediaServerModuleFixture()
      {
          m_camera.reset(new resource::test::CameraMock(&serverModule()));
          m_camera->setPhysicalId("motion archive test");
          m_dataProviderStub.reset(new DataProviderStub(m_camera));
      }

      void createTestData()
      {
          QnMetaDataV1Ptr motion(new QnMetaDataV1());
          for (int x = 0; x < Qn::kMotionGridWidth / 2; ++x)
          {
              for (int y = 0; y < Qn::kMotionGridHeight / 2; ++y)
                  motion->setMotionAt(x, y);
          }
          motion->dataProvider = m_dataProviderStub.get();

          auto baseDateMs =
              QDateTime::fromString("2017-12-31T00:23:50", Qt::ISODate).toMSecsSinceEpoch();
          for (qint64 i = 0; i < kSteps; ++i)
          {
              qint64 valueMs = baseDateMs + i * kDeltaMs;
              motion->timestamp = valueMs * 1000LL;
              m_numericDates.push_back(valueMs);
              serverModule().motionHelper()->saveToArchive(motion);
          }
          motion->timestamp += kDeltaMs * 1000LL;
          serverModule().motionHelper()->saveToArchive(motion); // flush data
      }

      QnChunksRequestData createRequest(Qt::SortOrder sortOrder) const
      {
          QnChunksRequestData request;
          request.resList << m_camera;
          QList<QRegion> rect;
          rect << QRegion(0, 0, Qn::kMotionGridWidth, Qn::kMotionGridHeight);
          request.filter = QJson::serialized(rect);
          request.periodsType = Qn::MotionContent;
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

TEST_F(MotionArchive, findMotionAsc)
{
    createTestData();
    QnChunksRequestData request = createRequest(Qt::SortOrder::AscendingOrder);

    checkData(serverModule().motionHelper()->matchImage(request), m_numericDates.size());

    request.limit = 50;
    checkData(serverModule().motionHelper()->matchImage(request), request.limit);

    request.detailLevel = std::chrono::milliseconds(kDeltaMs);
    checkData(serverModule().motionHelper()->matchImage(request), 1);
}

TEST_F(MotionArchive, findMotionDesc)
{
    createTestData();
    QnChunksRequestData request = createRequest(Qt::SortOrder::DescendingOrder);
    std::reverse(m_numericDates.begin(), m_numericDates.end());

    checkData(serverModule().motionHelper()->matchImage(request), m_numericDates.size());

    request.limit = 50;
    checkData(serverModule().motionHelper()->matchImage(request), request.limit);

    request.detailLevel = std::chrono::milliseconds(kDeltaMs);

    auto result = serverModule().motionHelper()->matchImage(request);
    ASSERT_EQ(1, result.size());
    ASSERT_EQ(m_numericDates[m_numericDates.size()-1], result[0].startTimeMs);
    auto deltaMs = m_numericDates[0] - m_numericDates[m_numericDates.size() - 1]
        + QnMotionArchive::kMinimalMotionDurationMs;
    ASSERT_EQ(deltaMs,result[0].durationMs);
}

} // nx::mediaserver::test
