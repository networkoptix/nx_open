#include <gtest/gtest.h>

#include <nx/fusion/serialization/json.h>
#include <nx/mediaserver/resource/camera_advanced_parameters_providers.h>
#include <utils/xml/camera_advanced_param_reader.h>

#include "camera_mock.h"

namespace nx {
namespace mediaserver {
namespace resource {
namespace test {

class CameraAdvancedParametersProviders: public CameraTest
{
public:
    template<typename T>
    static void expectJsonEq(const T& expected, const T& actual)
    {
        EXPECT_EQ(QJson::serialized(expected), QJson::serialized(actual));
    }

    template<typename T>
    static void expectContainerEq(const T& expected, const T& actual)
    {
        EXPECT_EQ(containerString(expected), containerString(actual));
    }
};

TEST_F(CameraAdvancedParametersProviders, ApiProviders)
{
    const auto camera = newCamera(
        [](CameraMock* camera)
        {
            camera->makeApiAdvancedParametersProvider<ApiMultiAdvancedParamitersProvider>({
                "a.x", "a.y", "b.m.x"});

            camera->makeApiAdvancedParametersProvider<ApiSingleAdvancedParamitersProvider>({
                "a.z", "b.m.y", "b.n.z", "c.d"});
        });
    NX_ASSERT(camera);

    const auto kExpectedDescriptions = CameraMock::makeParameterDescriptions({
        "a.x", "a.y", "a.z", "b.m.x", "b.m.y", "b.n.z", "c.d"});
    expectJsonEq(kExpectedDescriptions, QnCameraAdvancedParamsReader::paramsFromResource(camera));

    // Get/Set.
    expectContainerEq(QSet<QString>{"a.x", "a.y"},
        camera->setAdvancedParameters({{"a.x", "x"}, {"a.y", "y"}, {"a.q", "q"}}));

    expectContainerEq(QnCameraAdvancedParamValueMap{{"a.x", "x"}, {"b.m.x", "x"}},
        camera->getAdvancedParameters({"a.x", "b.m.x", "q.x"}));

    expectContainerEq(QSet<QString>{"b.m.x", "b.m.y"},
        camera->setAdvancedParameters({{"b.m.x", "x"}, {"b.m.y", "y"}, {"b.m.z", "z"}}));

    expectContainerEq(QnCameraAdvancedParamValueMap{{"a.x", "x"}, {"a.z", "z"}},
        camera->getAdvancedParameters({"a.x", "a.z", "a.q"}));
}

TEST_F(CameraAdvancedParametersProviders, StreamCapabilitiesDescriptions)
{
    const auto camera = newCamera(
        [](CameraMock* camera)
        {
            camera->setStreamCapabilityMaps({
                {{"MJPEG", QSize(800, 600)}, {10, 100, 30}},
                {{"MJPEG", QSize(1920, 1080)},{ 10, 100, 20}},
                {{"H264", QSize(800, 600)}, {10, 100, 60}},
                {{"H264", QSize(1920, 1080)}, {10, 100, 30}},
                {{"H265", QSize(800, 600)}, {10, 100, 30}},
                {{"H265", QSize(1920, 1080)}, {10, 100, 15}},
            },{
                {{"MJPEG", QSize(200, 150)}, {10, 100, 30}},
                {{"MJPEG", QSize(480, 270)}, {10, 100, 20}},
                {{"MJPEG", QSize(800, 600)}, {10, 100, 15}},
           });
        });
    NX_ASSERT(camera);

    // Default values for streams.
    {
        const auto streamParams = camera->advancedLiveStreamParams();
        EXPECT_EQ(streamParams.primaryStream.codec, QLatin1String("H264"));
        EXPECT_EQ(streamParams.primaryStream.resolution, QSize(1920, 1080));
        EXPECT_EQ(streamParams.primaryStream.bitrateKbps, 100);
        EXPECT_EQ(streamParams.primaryStream.fps, 30);
        EXPECT_EQ(streamParams.secondaryStream.codec, QLatin1String("MJPEG"));
        EXPECT_EQ(streamParams.secondaryStream.resolution, QSize(480, 270));
        EXPECT_EQ(streamParams.secondaryStream.bitrateKbps, 100);
        EXPECT_EQ(streamParams.secondaryStream.fps, 20);
    }

    // Advanced parameters descriptions.
    {
        const auto parameters = QnCameraAdvancedParamsReader::paramsFromResource(camera);
        EXPECT_EQ(parameters.name, QLatin1String("pramaryStreamConfiguration, secondaryStreamConfiguration"));
        ASSERT_EQ(parameters.groups.size(), 1);

        const auto videoStreams = parameters.groups.front();
        EXPECT_EQ(videoStreams.name, QLatin1String("Video Streams Configuration"));
        EXPECT_EQ(videoStreams.params.size(), 0);
        ASSERT_EQ(videoStreams.groups.size(), 2);

        const auto expectParam =
            [](const QnCameraAdvancedParameter& parameter,
                const char* id, const char* name, const char* range = nullptr)
            {
                EXPECT_EQ(parameter.id, QString::fromUtf8(id));
                EXPECT_EQ(parameter.name, QString::fromUtf8(name));
                if (range) { EXPECT_EQ(parameter.range, QString::fromUtf8(range)); }
            };

        const auto primary = videoStreams.groups.front();
        EXPECT_EQ(primary.name, QLatin1String("Primary"));
        EXPECT_EQ(primary.groups.size(), 0);
        ASSERT_EQ(primary.params.size(), 2);
        expectParam(primary.params[0], "primaryStream.codec", "Codec", "H264,H265,MJPEG");
        expectParam(primary.params[1], "primaryStream.resolution", "Resolution");

        const auto secondary = videoStreams.groups.back();
        EXPECT_EQ(secondary.name, QLatin1String("Secondary"));
        EXPECT_EQ(secondary.groups.size(), 0);
        ASSERT_EQ(secondary.params.size(), 4);
        expectParam(secondary.params[0], "secondaryStream.codec", "Codec", "MJPEG");
        expectParam(secondary.params[1], "secondaryStream.resolution", "Resolution");
        expectParam(secondary.params[2], "secondaryStream.bitrateKbps", "Bitrate (Kbps)");
        expectParam(secondary.params[3], "secondaryStream.fps", "Frames per Second (FPS)");
    }

    // Advanced parameters get/set.
    {
        // TODO
    }
}

} // namespace test
} // namespace resource
} // namespace mediaserver
} // namespace nx
