#include <gtest/gtest.h>

#include <QtCore/QSize>
#include <QtCore/QList>

#include <nx/vms/server/resource/camera.h>

namespace nx {
namespace test {

using namespace nx::vms::server;
using R = QSize;
using Resolution = R;
using ResolutionList = QList<Resolution>;

class SecondaryStreamResolutionTest: public ::testing::Test
{

public:
    void whenResolutionListIs(const ResolutionList& resolutionList)
    {
        m_resolutionList = resolutionList;
    }

    void thenSecondaryResolutionShouldBe(const Resolution& secondaryResolution)
    {
        const auto closestResolution = resource::Camera::closestResolution(
            SECONDARY_STREAM_DEFAULT_RESOLUTION,
            resource::Camera::getResolutionAspectRatio(m_resolutionList.front()),
            SECONDARY_STREAM_MAX_RESOLUTION,
            m_resolutionList);

        qDebug() << closestResolution;

        ASSERT_EQ(closestResolution, secondaryResolution);
    }

private:
    ResolutionList m_resolutionList;
};

TEST_F(SecondaryStreamResolutionTest, hasResolutionWithTheSameAspectRatio_16_9)
{
    whenResolutionListIs({
        R(1920,1080), R(1280, 960), R(1280, 720), R(1024, 768), R(800, 600), R(720, 567),
        R(640, 480), R(640, 360), R(320, 240)
    });
    thenSecondaryResolutionShouldBe(R(640, 360));
}

TEST_F(SecondaryStreamResolutionTest, noResolutionWithTheSameAspectRatio_16_9)
{
    whenResolutionListIs({
        R(1920,1080), R(1280, 960), R(1280, 720), R(1024, 768), R(800, 600), R(720, 567),
        R(640, 480), R(320, 240)
    });
    thenSecondaryResolutionShouldBe(R(640, 480));
}

TEST_F(SecondaryStreamResolutionTest, allResolutionsAreTooBig)
{
    whenResolutionListIs({R(1920,1080), R(1280, 960), R(1280, 720)});
    thenSecondaryResolutionShouldBe(R(0, 0));
}

TEST_F(SecondaryStreamResolutionTest, hasResolutionWithTheSameAspectRatio_4_3)
{
    whenResolutionListIs({R(1280, 960), R(1280, 720), R(1152, 648), R(1024, 576), R(720, 567),
        R(640, 480), R(640, 360)});
    thenSecondaryResolutionShouldBe(R(640, 480));
}

TEST_F(SecondaryStreamResolutionTest, chooseTheClosestToIdealResolutionWithTheSameAspectRatio)
{
    whenResolutionListIs({R(1280, 960), R(1280, 720), R(1024, 768), R(960, 720), R(800, 600)});
    thenSecondaryResolutionShouldBe(R(800, 600));
}

TEST_F(SecondaryStreamResolutionTest, noResolutionWithTheSameAspectRatio_4_3)
{
    whenResolutionListIs({R(1280, 960), R(1280, 720), R(1152, 648), R(1024, 576), R(720, 567),
        R(640, 360)});
    thenSecondaryResolutionShouldBe(R(640, 360));
}

} // namespace test
} // namespace nx

