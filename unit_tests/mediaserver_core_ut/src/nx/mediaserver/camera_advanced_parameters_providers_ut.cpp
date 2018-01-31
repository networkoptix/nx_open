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
    static void expectEq(const T& expected, const T& actual)
    {
        EXPECT_EQ(QJson::serialized(expected), QJson::serialized(actual));
    }

    static void expectEq(const QSet<QString>& expected, const QSet<QString>& actual)
    {
        EXPECT_EQ(containerString(expected), containerString(actual));
    }

    static void expectEq(
        const QnCameraAdvancedParamValueMap& expected, const QnCameraAdvancedParamValueMap& actual)
    {
        EXPECT_EQ(containerString(expected.toStdMap()), containerString(actual.toStdMap()));
    }

    static void expectVideoStreams(const QnCameraAdvancedParamGroup& videoStreams)
    {
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
        ASSERT_EQ(primary.params.size(), 3);
        expectParam(primary.params[0], "primaryStream.codec", "Codec", "H264,H265,MJPEG");
        expectParam(primary.params[1], "primaryStream.resolution", "Resolution");
        expectParam(primary.params[2], "primaryStream.resetToDefaults", "Reset to Defaults");

        const auto secondary = videoStreams.groups.back();
        EXPECT_EQ(secondary.name, QLatin1String("Secondary"));
        EXPECT_EQ(secondary.groups.size(), 0);
        ASSERT_EQ(secondary.params.size(), 5);
        expectParam(secondary.params[0], "secondaryStream.codec", "Codec", "MJPEG");
        expectParam(secondary.params[1], "secondaryStream.resolution", "Resolution");
        expectParam(secondary.params[2], "secondaryStream.bitrateKbps", "Bitrate");
        expectParam(secondary.params[3], "secondaryStream.fps", "FPS");
        expectParam(secondary.params[4], "secondaryStream.resetToDefaults", "Reset to Defaults");
    }

    static void expectLiveParams(QnSharedResourcePointer<Camera> camera,
        const char* primaryCodec, const QSize& primaryResolution,
        const char* secondaryCodec, const QSize& secondaryResolution,
        int secondaryBitrate, int secondaryFps)
    {
        const auto streamParams = camera->advancedLiveStreamParams();
        EXPECT_EQ(streamParams.primaryStream.codec, QString::fromUtf8(primaryCodec));
        EXPECT_EQ(toString(streamParams.primaryStream.resolution), toString(primaryResolution));
        EXPECT_EQ(streamParams.secondaryStream.codec, QString::fromUtf8(secondaryCodec));
        EXPECT_EQ(toString(streamParams.secondaryStream.resolution), toString(secondaryResolution));
        EXPECT_EQ(streamParams.secondaryStream.bitrateKbps, secondaryBitrate);
        EXPECT_EQ(streamParams.secondaryStream.fps, secondaryFps);
    }
};

TEST_F(CameraAdvancedParametersProviders, ApiProviders)
{
    const auto camera = newCamera(
        [](CameraMock* camera)
        {
            camera->makeApiAdvancedParametersProvider<ApiMultiAdvancedParametersProvider>({
                "a.x", "a.y", "b.m.x"});

            camera->makeApiAdvancedParametersProvider<ApiSingleAdvancedParametersProvider>({
                "a.z", "b.m.y", "b.n.z", "c.d"});
        });
    NX_ASSERT(camera);

    // Descriptions.
    {
        auto expectedDescriptions = CameraMock::makeParameterDescriptions({
            "a.x", "a.y", "a.z", "b.m.x", "b.m.y", "b.n.z", "c.d"});
        expectedDescriptions.name = lit("makeParameterDescriptions, makeParameterDescriptions");
        expectedDescriptions.version = lit("1.0, 1.0");
        expectedDescriptions.unique_id = lit("X, X");
        expectEq(expectedDescriptions, QnCameraAdvancedParamsReader::paramsFromResource(camera));
    }

    // Get/Set.
    expectEq(QSet<QString>{"a.x", "a.y"},
        camera->setAdvancedParameters({{"a.x", "x"}, {"a.y", "y"}, {"a.q", "q"}}));

    expectEq(QnCameraAdvancedParamValueMap{{"a.x", "x"}, {"b.m.x", "default"}},
        camera->getAdvancedParameters({"a.x", "b.m.x", "q.x"}));

    expectEq(QSet<QString>{"b.m.x", "b.m.y"},
        camera->setAdvancedParameters({{"b.m.x", "x"}, {"b.m.y", "y"}, {"b.m.z", "z"}}));

    expectEq(QnCameraAdvancedParamValueMap{{"b.m.x", "x"}, {"b.m.y", "y"}, {"c.d", "default"}},
        camera->getAdvancedParameters({"b.m.x", "b.m.y", "c.d"}));
}

TEST_F(CameraAdvancedParametersProviders, StreamCapabilities)
{
    const auto camera = newCamera(
        [](CameraMock* camera)
        {
            camera->setStreamCapabilityMaps({
                {{"MJPEG", QSize(800, 600)}, {10, 100, 30}},
                {{"MJPEG", QSize(1920, 1080)}, { 10, 100, 20}},
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

    // Advanced parameters descriptions.
    const auto descriptions = QnCameraAdvancedParamsReader::paramsFromResource(camera);
    EXPECT_EQ(descriptions.name, QLatin1String("primaryStreamConfiguration, secondaryStreamConfiguration"));
    ASSERT_EQ(descriptions.groups.size(), 1);
    expectVideoStreams(descriptions.groups.front());


    // Default values for streams.
    expectLiveParams(camera, "H264", QSize(1920, 1080), "MJPEG", QSize(480, 270), 192, 7);
    EXPECT_TRUE(camera->getProperty(QString("primaryStreamConfiguration")).isEmpty());
    EXPECT_TRUE(camera->getProperty(QString("secondaryStreamConfiguration")).isEmpty());

    // Advanced parameters get/set.
    expectEq(QnCameraAdvancedParamValueMap{
            {"primaryStream.codec", "H264"},
            {"secondaryStream.resolution", "480x270"},
            {"secondaryStream.bitrateKbps", "192"}},
        camera->getAdvancedParameters(
            {"primaryStream.codec", "secondaryStream.resolution", "secondaryStream.bitrateKbps"}));

    expectEq(QSet<QString>{"secondaryStream.resolution", "secondaryStream.fps"},
        camera->setAdvancedParameters({
            {"primaryStream.codec", "MPEG4"}, //< Codec is not supported.
            {"secondaryStream.resolution", "800x600"},
            {"secondaryStream.fps", "15"}}));

    expectLiveParams(camera, "H264", QSize(1920, 1080), "MJPEG", QSize(800, 600), 192, 15);
    EXPECT_TRUE(camera->getProperty(QString("primaryStreamConfiguration")).isEmpty());
    EXPECT_EQ(QString(
            R"json({"bitrateKbps":192,"codec":"MJPEG","fps":15,"quality":"6","resolution":{"height":600,"width":800}})json"),
        camera->getProperty(QString("secondaryStreamConfiguration")));

    // Brocken setProperty().
    camera->enableSetProperty(false);
    {
        expectEq(QSet<QString>{}, //< Failed to setProperty().
            camera->setAdvancedParameters({{"primaryStream.resolution", "800x600"}}));
        expectEq(QSet<QString>{"primaryStream.resolution"}, //< Property is already set.
            camera->setAdvancedParameters({{"primaryStream.resolution", "1920x1080"}}));
    }

    // Reset to defaults.
    camera->enableSetProperty(true);
    expectEq(QSet<QString>{"secondaryStream.resetToDefaults"},
        camera->setAdvancedParameters({{"secondaryStream.resetToDefaults", "true"}}));

    expectLiveParams(camera, "H264", QSize(1920, 1080), "MJPEG", QSize(480, 270), 192, 7);
    EXPECT_TRUE(camera->getProperty(QString("primaryStreamConfiguration")).isEmpty());
    EXPECT_TRUE(camera->getProperty(QString("secondaryStreamConfiguration")).isEmpty());
}

TEST_F(CameraAdvancedParametersProviders, MixedUpParameters)
{
    const auto camera = newCamera(
        [](CameraMock* camera)
        {
            camera->makeApiAdvancedParametersProvider<ApiMultiAdvancedParametersProvider>({
                "testGroup1.testParam1", "testGroup1.testParam2"});

            camera->makeApiAdvancedParametersProvider<ApiSingleAdvancedParametersProvider>({
                "testGroup2.testParam3"});

            camera->setStreamCapabilityMaps({
                {{"MJPEG", QSize(1920, 1080)}, { 10, 100, 20}},
                {{"H264", QSize(1920, 1080)}, {10, 100, 30}},
                {{"H265", QSize(1920, 1080)}, {10, 100, 15}},
            },{
                {{"MJPEG", QSize(800, 600)}, {10, 100, 15}},
            });
        });
    NX_ASSERT(camera);

    const auto parameters = QnCameraAdvancedParamsReader::paramsFromResource(camera);
    EXPECT_EQ(parameters.name, QLatin1String(
        "primaryStreamConfiguration, secondaryStreamConfiguration, makeParameterDescriptions, makeParameterDescriptions"));
    ASSERT_EQ(parameters.groups.size(), 3);

    const auto videoStreams = parameters.groups[0];
    expectVideoStreams(videoStreams);

    const auto testGroup1 = parameters.groups[1];
    EXPECT_EQ(testGroup1.name, "testGroup1");
    EXPECT_EQ(testGroup1.groups.size(), 0);
    ASSERT_EQ(testGroup1.params.size(), 2);
    EXPECT_EQ(testGroup1.params[0].name, lit("testGroup1.testParam1"));
    EXPECT_EQ(testGroup1.params[1].name, lit("testGroup1.testParam2"));

    const auto testGroup2 = parameters.groups[2];
    EXPECT_EQ(testGroup2.name, "testGroup2");
    EXPECT_EQ(testGroup2.groups.size(), 0);
    ASSERT_EQ(testGroup2.params.size(), 1);
    EXPECT_EQ(testGroup2.params[0].name, lit("testGroup2.testParam3"));
}

} // namespace test
} // namespace resource
} // namespace mediaserver
} // namespace nx
