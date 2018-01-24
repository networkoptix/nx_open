#include <gtest/gtest.h>

#include <nx/fusion/serialization/json.h>
#include <nx/mediaserver/resource/camera_advanced_parameters_providers.h>
#include <utils/xml/camera_advanced_param_reader.h>

#include "camera_mock.h"

namespace nx {
namespace mediaserver {
namespace resource {
namespace test {

class LiveStreamParameters: public CameraTest
{
public:
    LiveStreamParameters()
    {
        m_camera = newCamera(
            [](CameraMock* camera)
        {
            camera->setStreamCapabilityMaps({
                { { "MJPEG", QSize(800, 600) },{ 10, 100, 30 } },
                { { "MJPEG", QSize(1920, 1080) },{ 10, 100, 20 } },
                { { "H264", QSize(800, 600) },{ 10, 100, 60 } },
                { { "H264", QSize(1920, 1080) },{ 10, 100, 30 } },
                { { "H265", QSize(800, 600) },{ 10, 100, 30 } },
                { { "H265", QSize(1920, 1080) },{ 10, 100, 15 } },
            }, {
                { { "MJPEG", QSize(200, 150) },{ 10, 100, 30 } },
                { { "MJPEG", QSize(480, 270) },{ 10, 100, 20 } },
                { { "MJPEG", QSize(800, 600) },{ 10, 100, 15 } },
            });
        });
        NX_ASSERT(m_camera);
        m_camera->setId(QnUuid::createUuid());

        m_primaryProvider.reset(dynamic_cast<QnLiveStreamProvider*>(m_camera->createDataProvider(Qn::CR_LiveVideo)));
        m_secondaryProvider.reset(dynamic_cast<QnLiveStreamProvider*>(m_camera->createDataProvider(Qn::CR_SecondaryLiveVideo)));
        NX_ASSERT(m_primaryProvider);
        NX_ASSERT(m_secondaryProvider);
    }

    QnSharedResourcePointer<CameraMock> m_camera;
    std::unique_ptr<QnLiveStreamProvider> m_primaryProvider;
    std::unique_ptr<QnLiveStreamProvider> m_secondaryProvider;
};

TEST_F(LiveStreamParameters, mergeWithEmptyParams)
{
    QnLiveStreamParams params;
    m_primaryProvider->setPrimaryStreamParams(params);

    auto updatedParams = m_primaryProvider->getLiveParams();
    EXPECT_EQ(QSize(1920, 1080), updatedParams.resolution);
    EXPECT_EQ(15, updatedParams.fps);
    EXPECT_EQ("H264", updatedParams.codec);
    EXPECT_GE(updatedParams.bitrateKbps, 1000);

    auto secondaryParams = m_secondaryProvider->getLiveParams();

    EXPECT_EQ(QSize(480, 270), secondaryParams.resolution);
    EXPECT_EQ(7, secondaryParams.fps);
    EXPECT_EQ("MJPEG", secondaryParams.codec);
    EXPECT_GE(secondaryParams.bitrateKbps, 100);
}


TEST_F(LiveStreamParameters, mergeWithNonEmptyParams)
{
    QnLiveStreamParams params;
    params.fps = 12;
    params.resolution = QSize(2536, 1080);
    params.codec = "H265";
    params.quality = Qn::QualityHighest;
    m_primaryProvider->setPrimaryStreamParams(params);

    auto updatedParams = m_primaryProvider->getLiveParams();
    EXPECT_EQ(QSize(2536, 1080), updatedParams.resolution);
    EXPECT_EQ(12, updatedParams.fps);
    EXPECT_EQ("H265", updatedParams.codec);
    EXPECT_GE(updatedParams.bitrateKbps, 2000);

    auto secondaryParams = m_secondaryProvider->getLiveParams();

    EXPECT_EQ(QSize(480, 270), secondaryParams.resolution);
    EXPECT_EQ(7, secondaryParams.fps);
    EXPECT_EQ("MJPEG", secondaryParams.codec);
    EXPECT_GE(secondaryParams.bitrateKbps, 100);

    params.bitrateKbps = 1000;
    m_primaryProvider->setPrimaryStreamParams(params);
    updatedParams = m_primaryProvider->getLiveParams();
    EXPECT_EQ(updatedParams.bitrateKbps, 1000);

}

} // namespace test
} // namespace resource
} // namespace mediaserver
} // namespace nx
