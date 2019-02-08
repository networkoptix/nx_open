#include <gtest/gtest.h>

#include <nx/fusion/serialization/json.h>
#include <nx/vms/server/resource/camera_advanced_parameters_providers.h>
#include <utils/xml/camera_advanced_param_reader.h>

#include "camera_mock.h"

namespace nx {
namespace vms::server {
namespace resource {
namespace test {

template <typename T>
std::set<T> toStdSet(const QSet<T> data)
{
    std::set<T> result;
    for (const auto& value: data)
        result.insert(value);
    return result;
}

std::vector<QSize> parseRange(const QString& rangeString)
{
    std::vector<QSize> result;
    const auto split = rangeString.splitRef(L',');
    for (const auto& resString : split)
    {
        const auto resSplit = resString.split(L'x');
        if (resSplit.size() != 2)
            continue;

        const auto width = resSplit[0].toInt();
        if (width == 0)
            continue;

        const auto height = resSplit[1].toInt();
        if (height == 0)
            continue;

        result.emplace_back(width, height);
    }

    std::sort(
        result.begin(),
        result.end(),
        [](const QSize& f, const QSize& s)
        {
            return f.width() * f.height() < s.width() * s.height();
        });

    return result;
}

boost::optional<double> aspectRatio(const QString& resolutionString)
{
    const auto split = resolutionString.splitRef(L'x');
    if (split.size() != 2)
        return boost::none;

    bool ok = false;
    const auto width = split[0].toInt(&ok);
    if (!ok)
        return boost::none;

    const auto height = split[1].toInt(&ok);
    if (!ok || height == 0)
        return boost::none;

    return width / (double) height;
}

std::vector<double> parseAspectRatioFromRange(const QString& rangeString)
{
    const auto resolutionStrings = rangeString.split(L',');
    std::vector<double> result;

    for (const auto& resolutionString: resolutionStrings)
    {
        const auto aspect = aspectRatio(resolutionString);
        if (aspect == boost::none)
            continue;

        result.push_back(*aspect);
    }

    std::sort(result.begin(), result.end());
    return result;
}

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
        EXPECT_EQ(containerString(toStdSet(expected)), containerString(toStdSet(actual)));
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
    EXPECT_EQ(descriptions.name,
        QLatin1String("primaryStreamConfiguration, secondaryStreamConfiguration"));
    ASSERT_EQ(descriptions.groups.size(), 1);
    expectVideoStreams(descriptions.groups.front());

    // Default values for streams.
    expectLiveParams(camera, "H264", QSize(1920, 1080), "MJPEG", QSize(480, 270), 262, 7);
    EXPECT_TRUE(camera->getProperty(QString("primaryStreamConfiguration")).isEmpty());
    EXPECT_TRUE(camera->getProperty(QString("secondaryStreamConfiguration")).isEmpty());

    // Advanced parameters get/set.
    expectEq(QnCameraAdvancedParamValueMap{
            {"primaryStream.codec", "H264"},
            {"secondaryStream.resolution", "480x270"},
            {"secondaryStream.bitrateKbps", "262"}},
        camera->getAdvancedParameters(
            {"primaryStream.codec", "secondaryStream.resolution", "secondaryStream.bitrateKbps"}));

    expectEq(QSet<QString>{"secondaryStream.resolution", "secondaryStream.fps"},
        camera->setAdvancedParameters({
            {"primaryStream.codec", "MPEG4"}, //< Codec is not supported.
            {"secondaryStream.resolution", "800x600"},
            {"secondaryStream.fps", "15"}}));

    expectLiveParams(camera, "H264", QSize(1920, 1080), "MJPEG", QSize(800, 600), 262, 15);
    EXPECT_TRUE(camera->getProperty(QString("primaryStreamConfiguration")).isEmpty());
    EXPECT_EQ(QString(
            R"json({"bitrateKbps":262,"codec":"MJPEG","fps":15,"quality":"undefined","resolution":{"height":600,"width":800}})json"),
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

    expectLiveParams(camera, "H264", QSize(1920, 1080), "MJPEG", QSize(480, 270), 262, 7);
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

TEST_F(CameraAdvancedParametersProviders, SameAspectRatioRestrictions)
{
    const auto camera = newCamera(
        [](CameraMock* camera)
        {
            camera->setMediaTraits({{lit("aspectRatioDependent"), {}}});
            camera->setStreamCapabilityMaps(
                {
                    {{lit("H264"), QSize(1920, 1080)}, nx::media::CameraStreamCapability()},
                    {{lit("H264"), QSize(1280, 960)}, nx::media::CameraStreamCapability()},
                    {{lit("H264"), QSize(1280, 720)}, nx::media::CameraStreamCapability()},
                    {{lit("H264"), QSize(800, 600)}, nx::media::CameraStreamCapability()},
                    {{lit("H264"), QSize(640, 480)}, nx::media::CameraStreamCapability()},
                    {{lit("H264"), QSize(640, 360)}, nx::media::CameraStreamCapability()},

                    {{lit("H265"), QSize(1920, 1080)}, nx::media::CameraStreamCapability()},
                    {{lit("H265"), QSize(1280, 960)}, nx::media::CameraStreamCapability()},
                    {{lit("H265"), QSize(1280, 720)}, nx::media::CameraStreamCapability()},
                    {{lit("H265"), QSize(800, 600)}, nx::media::CameraStreamCapability()},
                    {{lit("H265"), QSize(640, 480)}, nx::media::CameraStreamCapability()},
                    {{lit("H265"), QSize(640, 360)}, nx::media::CameraStreamCapability()}
                },
                {
                    {{lit("H264"), QSize(1920, 1080)}, nx::media::CameraStreamCapability()},
                    {{lit("H264"), QSize(1280, 960)}, nx::media::CameraStreamCapability()},
                    {{lit("H264"), QSize(1280, 720)}, nx::media::CameraStreamCapability()},
                    {{lit("H264"), QSize(800, 600)}, nx::media::CameraStreamCapability()},
                    {{lit("H264"), QSize(640, 480)}, nx::media::CameraStreamCapability()},
                    {{lit("H264"), QSize(640, 360)}, nx::media::CameraStreamCapability()},

                    {{lit("H265"), QSize(1920, 1080)}, nx::media::CameraStreamCapability()},
                    {{lit("H265"), QSize(1280, 960)}, nx::media::CameraStreamCapability()},
                    {{lit("H265"), QSize(1280, 720)}, nx::media::CameraStreamCapability()},
                    {{lit("H265"), QSize(800, 600)}, nx::media::CameraStreamCapability()},
                    {{lit("H265"), QSize(640, 480)}, nx::media::CameraStreamCapability()},
                    {{lit("H265"), QSize(640, 360)}, nx::media::CameraStreamCapability()},
                });
        });
    NX_ASSERT(camera);

    // Advanced parameters descriptions.
    const auto descriptions = QnCameraAdvancedParamsReader::paramsFromResource(camera);
    auto resolutionParameter = descriptions.getParameterById("secondaryStream.resolution");

    ASSERT_TRUE(resolutionParameter.isValid());
    bool gotAspectRatioDependencies = false;
    for (const auto& dependency: resolutionParameter.dependencies)
    {
        if (!dependency.id.startsWith(lit("aspectRatio")))
            continue;

        gotAspectRatioDependencies = true;
        bool gotAspectRatioConditions = false;
        const auto secondaryResolutionList = parseRange(dependency.range);
        ASSERT_FALSE(secondaryResolutionList.empty());
        const auto secondaryAspectRatio = parseAspectRatioFromRange(dependency.range);
        ASSERT_FALSE(secondaryAspectRatio.empty());
        ASSERT_EQ(secondaryResolutionList.size(), secondaryAspectRatio.size());
        ASSERT_DOUBLE_EQ(*secondaryAspectRatio.begin(), *secondaryAspectRatio.rbegin());

        for (const auto& condition: dependency.conditions)
        {
            if (condition.paramId != lit("primaryStream.resolution"))
                continue;

            ASSERT_EQ(
                condition.type, QnCameraAdvancedParameterCondition::ConditionType::inRange);

            const auto primaryResolutionList = parseRange(condition.value);
            const auto primaryAspectRatio = parseAspectRatioFromRange(condition.value);
            gotAspectRatioConditions = true;

            ASSERT_TRUE(
                std::equal(
                    primaryResolutionList.begin(),
                    primaryResolutionList.end(),
                    secondaryResolutionList.begin()));
        }

        ASSERT_TRUE(gotAspectRatioConditions);
    }
    ASSERT_TRUE(gotAspectRatioDependencies);
}

// TODO: #dmishin add more test cases for aspect ratio dependent resolutions.

TEST_F(CameraAdvancedParametersProviders, AdvancedParametersEquality)
{
    auto camera = newCamera([](CameraMock* /*camera*/) {});
    auto parameters = QnCameraAdvancedParamsReader::paramsFromResource(camera);
    ASSERT_EQ(parameters, QnCameraAdvancedParams());

    QnCameraAdvancedParamsReader::setParamsToResource(camera, QnCameraAdvancedParams());
    parameters = QnCameraAdvancedParamsReader::paramsFromResource(camera);
    ASSERT_EQ(parameters, QnCameraAdvancedParams());

    QnCameraAdvancedParams testParameters;
    testParameters.name = lit("Some name");
    testParameters.version = lit("0");
    testParameters.unique_id = lit("Some id");
    testParameters.packet_mode = false;

    QnCameraAdvancedParamsReader::setParamsToResource(camera, testParameters);
    parameters = QnCameraAdvancedParamsReader::paramsFromResource(camera);
    ASSERT_EQ(parameters, testParameters);

    QnCameraAdvancedParamsReader::setParamsToResource(camera, QnCameraAdvancedParams());
    parameters = QnCameraAdvancedParamsReader::paramsFromResource(camera);
    ASSERT_EQ(parameters, QnCameraAdvancedParams());
}

} // namespace test
} // namespace resource
} // namespace vms::server
} // namespace nx
