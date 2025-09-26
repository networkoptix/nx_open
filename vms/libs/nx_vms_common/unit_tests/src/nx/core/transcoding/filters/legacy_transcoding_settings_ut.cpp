// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <core/resource/media_resource.h>
#include <core/resource/resource_media_layout.h>
#include <nx/core/transcoding/filters/media_settings_to_transcoding.h>
#include <nx/fusion/model_functions.h>
#include <nx/network/rest/params.h>
#include <nx/reflect/json.h>
#include <nx/reflect/string_conversion.h>
#include <nx/vms/api/data/media_settings.h>

class TestVideoLayout: public QnDefaultResourceVideoLayout
{
public:
    virtual int channelCount() const override
    {
        return videoChannelCount;
    }
    int videoChannelCount = 1;
};

void isEqual(const nx::core::transcoding::Settings& left, const nx::core::transcoding::Settings& right)
{
    EXPECT_EQ(left.aspectRatio, right.aspectRatio);
    EXPECT_EQ(left.rotation, right.rotation);
    EXPECT_EQ(left.zoomWindow, right.zoomWindow);
    EXPECT_EQ(left.enhancement, right.enhancement);
    EXPECT_EQ(left.dewarping, right.dewarping);
    EXPECT_EQ(left.watermark, right.watermark);
}

std::string toString(nx::vms::api::MediaSettings::ValidationResult result)
{
    using namespace nx::vms::api;
    switch (result)
    {
        case MediaSettings::ValidationResult::nullId:
            return "nullId";
        case MediaSettings::ValidationResult::invalidStreamIndex:
            return "invalidStreamIndex";
        case MediaSettings::ValidationResult::invalidRotation:
            return "invalidRotation";
        case MediaSettings::ValidationResult::invalidDewarpingPanofactor:
            return "invalidDewarpingPanofactor";
        case MediaSettings::ValidationResult::invalidZoom:
            return "invalidZoom";
        case MediaSettings::ValidationResult::isValid:
            return "isValid";
        default:
            NX_CRITICAL(false, "Can't be here");
            return "";
    }
}

void checkResourceAndJson(
    const nx::network::rest::Params& params,
    const nx::core::transcoding::Settings& expected,
    const std::string& expectedError)
{
    nx::vms::api::MediaSettings settings;
    try
    {
        settings = QJson::deserializeOrThrow<nx::vms::api::MediaSettings>(
            params.toJson(/*excludeCommon*/ true),
            /*allowStringConversions*/ true);
    }
    catch (const nx::json::InvalidParameterException& e)
    {
        EXPECT_EQ(std::string(e.what()), expectedError);
        return;
    }
    catch (const nx::json::InvalidJsonException& e)
    {
        EXPECT_EQ(std::string(e.what()), expectedError);
        return;
    }
    catch (const std::exception& /*e*/)
    {
        EXPECT_TRUE(false);
        return;
    }
    auto validationResult = settings.validateMediaSettings();
    if (validationResult != nx::vms::api::MediaSettings::ValidationResult::isValid)
    {
        EXPECT_EQ(toString(validationResult), expectedError);
        return;
    }
    nx::core::transcoding::Settings transcodingSettings = nx::core::transcoding::fromMediaSettings(
        expected.aspectRatio,
        expected.rotation,
        expected.layout,
        nx::vms::api::dewarping::MediaData(),
        settings);

    isEqual(transcodingSettings, expected);
}

TEST(TranscodingSettings, emptyParams)
{
    nx::network::rest::Params params{};
    nx::core::transcoding::Settings settings;
    checkResourceAndJson(params, settings, "");
}

TEST(TranscodingSettings, resolutionDontMatters)
{
    nx::network::rest::Params params{
        {"resolution", "640x480"}};
    nx::core::transcoding::Settings settings;
    settings.layout = QnConstResourceVideoLayoutPtr(new TestVideoLayout());
    checkResourceAndJson(params, settings, "");
}

TEST(TranscodingSettings, badResolutionArgument)
{
    nx::network::rest::Params params{
        {"resolution", "xxx"}};
    nx::core::transcoding::Settings settings;
    settings.layout = QnConstResourceVideoLayoutPtr(new TestVideoLayout());
    checkResourceAndJson(params, settings, "Invalid parameter 'resolution': \"xxx\"");
}

TEST(TranscodingSettings, resolutionThenTranscodingDontMatters)
{
    nx::network::rest::Params params{
        {"resolutionWhenTranscoding", "640x480"}};
    nx::core::transcoding::Settings settings;
    settings.layout = QnConstResourceVideoLayoutPtr(new TestVideoLayout());
    checkResourceAndJson(params, settings, "");
}

TEST(TranscodingSettings, badResolutionThenTranscodingArgument)
{
    nx::network::rest::Params params{
        {"resolutionThenTranscoding", "xxx"}};
    nx::core::transcoding::Settings settings;
    settings.layout = QnConstResourceVideoLayoutPtr(new TestVideoLayout());
    checkResourceAndJson(params, settings, "Invalid parameter 'resolutionThenTranscoding': \"xxx\"");
}

TEST(TranscodingSettings, streamNotMatters)
{
    {
        nx::network::rest::Params params{
            {"stream", "0"}};
        nx::core::transcoding::Settings settings;
        settings.layout = QnConstResourceVideoLayoutPtr(new TestVideoLayout());
        checkResourceAndJson(params, settings, "");
    }
    {
        nx::network::rest::Params params{
            {"stream", "1"}};
        nx::core::transcoding::Settings settings;
        settings.layout = QnConstResourceVideoLayoutPtr(new TestVideoLayout());
        checkResourceAndJson(params, settings, "");
    }
    {
        nx::network::rest::Params params{
            {"stream", "-1"}};
        nx::core::transcoding::Settings settings;
        settings.layout = QnConstResourceVideoLayoutPtr(new TestVideoLayout());
        checkResourceAndJson(params, settings, "");
    }
    {
        nx::network::rest::Params params{
            {"stream", "primary"}};
        nx::core::transcoding::Settings settings;
        settings.layout = QnConstResourceVideoLayoutPtr(new TestVideoLayout());
        checkResourceAndJson(params, settings, "");
    }
    {
        nx::network::rest::Params params{
            {"stream", "secondary"}};
        nx::core::transcoding::Settings settings;
        settings.layout = QnConstResourceVideoLayoutPtr(new TestVideoLayout());
        checkResourceAndJson(params, settings, "");
    }
    {
        nx::network::rest::Params params{
            {"stream", ""}};
        nx::core::transcoding::Settings settings;
        settings.layout = QnConstResourceVideoLayoutPtr(new TestVideoLayout());
        checkResourceAndJson(params, settings, "");
    }
}

TEST(TranscodingSettings, streamInvalid)
{
    {
        nx::network::rest::Params params{
            {"stream", "invalid"}};
        nx::core::transcoding::Settings settings;
        settings.layout = QnConstResourceVideoLayoutPtr(new TestVideoLayout());
        checkResourceAndJson(params, settings, "Invalid parameter 'stream': \"invalid\"");
    }
    {
        nx::network::rest::Params params{
            {"stream", "2"}};
        nx::core::transcoding::Settings settings;
        settings.layout = QnConstResourceVideoLayoutPtr(new TestVideoLayout());
        checkResourceAndJson(params, settings, "invalidStreamIndex");
    }
}

TEST(TranscodingSettings, rotationMatters)
{
    {
        nx::network::rest::Params params{
            {"rotation", "0"}};
        nx::core::transcoding::Settings settings;
        settings.layout = QnConstResourceVideoLayoutPtr(new TestVideoLayout());
        checkResourceAndJson(params, settings, "");
    }
    {
        nx::network::rest::Params params{
            {"rotation", "90"}};
        nx::core::transcoding::Settings settings;
        settings.layout = QnConstResourceVideoLayoutPtr(new TestVideoLayout());
        settings.rotation = 90;
        checkResourceAndJson(params, settings, "");
    }
    {
        nx::network::rest::Params params{
            {"rotation", "auto"}};
        nx::core::transcoding::Settings settings;
        settings.layout = QnConstResourceVideoLayoutPtr(new TestVideoLayout());
        checkResourceAndJson(params, settings, "");
    }
    {
        nx::network::rest::Params params{
            {"rotation", "auto"}};
        nx::core::transcoding::Settings settings;
        settings.layout = QnConstResourceVideoLayoutPtr(new TestVideoLayout());
        settings.rotation = 90;
        checkResourceAndJson(params, settings, "");
    }
    {
        nx::network::rest::Params params{
            {"rotation", "90"}};
        nx::core::transcoding::Settings settings;
        settings.layout = QnConstResourceVideoLayoutPtr(new TestVideoLayout());
        settings.rotation = 90;
        checkResourceAndJson(params, settings, "");
    }
}

TEST(TranscodingSettings, badRotationArgument)
{
    nx::network::rest::Params params{
        {"rotation", "-90"}};
    nx::core::transcoding::Settings settings;
    settings.layout = QnConstResourceVideoLayoutPtr(new TestVideoLayout());
    checkResourceAndJson(params, settings, "invalidRotation");
}

TEST(TranscodingSettings, aspectRatioMatters)
{
    {
        nx::network::rest::Params params{
            {"aspectRatio", "0:0"}};
        nx::core::transcoding::Settings settings;
        settings.layout = QnConstResourceVideoLayoutPtr(new TestVideoLayout());
        checkResourceAndJson(params, settings, "");
    }
    {
        nx::network::rest::Params params{
            {"aspectRatio", "4:3"}};
        nx::core::transcoding::Settings settings;
        settings.layout = QnConstResourceVideoLayoutPtr(new TestVideoLayout());
        settings.aspectRatio = QnAspectRatio(4, 3);
        checkResourceAndJson(params, settings, "");
    }
    {
        nx::network::rest::Params params{
            {"aspectRatio", "auto"}};
        nx::core::transcoding::Settings settings;
        settings.layout = QnConstResourceVideoLayoutPtr(new TestVideoLayout());
        checkResourceAndJson(params, settings, "");
    }
    {
        nx::network::rest::Params params{
            {"aspectRatio", "auto"}};
        nx::core::transcoding::Settings settings;
        settings.layout = QnConstResourceVideoLayoutPtr(new TestVideoLayout());
        settings.aspectRatio = QnAspectRatio(4, 3);
        checkResourceAndJson(params, settings, "");
    }
    {
        nx::network::rest::Params params{
            {"aspectRatio", "4:3"}};
        nx::core::transcoding::Settings settings;
        settings.layout = QnConstResourceVideoLayoutPtr(new TestVideoLayout());
        settings.aspectRatio = QnAspectRatio(4, 3);
        checkResourceAndJson(params, settings, "");
    }
}

TEST(TranscodingSettings, badAspectRatioArgument)
{
    {
        nx::network::rest::Params params{
            {"aspectRatio", "-90"}};
        nx::core::transcoding::Settings settings;
        settings.layout = QnConstResourceVideoLayoutPtr(new TestVideoLayout());
        checkResourceAndJson(params, settings, "Invalid parameter 'aspectRatio': \"-90\"");
    }
    {
        nx::network::rest::Params params{
            {"aspectRatio", "x:y"}};
        nx::core::transcoding::Settings settings;
        settings.layout = QnConstResourceVideoLayoutPtr(new TestVideoLayout());
        checkResourceAndJson(params, settings, "Invalid parameter 'aspectRatio': \"x:y\"");
    }
}

TEST(TranscodingSettings, dewarpingMatters)
{
    {
        nx::network::rest::Params params{
            {"dewarping", "0"}};
        nx::core::transcoding::Settings settings;
        settings.layout = QnConstResourceVideoLayoutPtr(new TestVideoLayout());
        checkResourceAndJson(params, settings, "");
    }
    {
        nx::network::rest::Params params{
            {"dewarping", "1"}};
        nx::core::transcoding::Settings settings;
        settings.layout = QnConstResourceVideoLayoutPtr(new TestVideoLayout());
        settings.dewarping.enabled = true;
        checkResourceAndJson(params, settings, "");
    }
    {
        nx::network::rest::Params params{
            {"dewarping", "true"},
            {"dewarpingXangle", "0.5"},
            {"dewarpingYangle", "0.3"},
            {"dewarpingFov", "0.4"},
            {"dewarpingPanofactor", "2"}};
        nx::core::transcoding::Settings settings;
        settings.layout = QnConstResourceVideoLayoutPtr(new TestVideoLayout());
        settings.dewarping.enabled = true;
        settings.dewarping.xAngle = 0.5;
        settings.dewarping.yAngle = 0.3;
        settings.dewarping.fov = 0.4;
        settings.dewarping.panoFactor = 2;
        checkResourceAndJson(params, settings, "");
    }
}

TEST(TranscodingSettings, badDewarpingArgument)
{
    {
        nx::network::rest::Params params{
            {"dewarping", "42"}};
        nx::core::transcoding::Settings settings;
        settings.layout = QnConstResourceVideoLayoutPtr(new TestVideoLayout());
        checkResourceAndJson(params, settings, "Invalid parameter 'dewarping': \"42\"");
    }
    {
        nx::network::rest::Params params{
            {"dewarping", "true"},
            {"dewarpingXangle", "0.5"},
            {"dewarpingYangle", "0.3"},
            {"dewarpingFov", "0.4"},
            {"dewarpingPanofactor", "3"}};
        nx::core::transcoding::Settings settings;
        settings.layout = QnConstResourceVideoLayoutPtr(new TestVideoLayout());
        checkResourceAndJson(params, settings, "invalidDewarpingPanofactor");
    }
    {
        nx::network::rest::Params params{
            {"dewarping", "true"},
            {"dewarpingXangle", "xxx"},
            {"dewarpingYangle", "0.3"},
            {"dewarpingFov", "0.4"},
            {"dewarpingPanofactor", "2"}};
        nx::core::transcoding::Settings settings;
        settings.layout = QnConstResourceVideoLayoutPtr(new TestVideoLayout());
        checkResourceAndJson(params, settings, "Invalid parameter 'dewarpingXangle': \"xxx\"");
    }
    {
        nx::network::rest::Params params{
            {"dewarping", "true"},
            {"dewarpingXangle", "0.5"},
            {"dewarpingYangle", "xxx"},
            {"dewarpingFov", "0.4"},
            {"dewarpingPanofactor", "2"}};
        nx::core::transcoding::Settings settings;
        settings.layout = QnConstResourceVideoLayoutPtr(new TestVideoLayout());
        checkResourceAndJson(params, settings, "Invalid parameter 'dewarpingYangle': \"xxx\"");
    }
    {
        nx::network::rest::Params params{
            {"dewarping", "true"},
            {"dewarpingXangle", "0.5"},
            {"dewarpingYangle", "0.3"},
            {"dewarpingFov", "xxx"},
            {"dewarpingPanofactor", "2"}};
        nx::core::transcoding::Settings settings;
        settings.layout = QnConstResourceVideoLayoutPtr(new TestVideoLayout());
        checkResourceAndJson(params, settings, "Invalid parameter 'dewarpingFov': \"xxx\"");
    }
}

TEST(TranscodingSettings, zoomMatters)
{
    nx::network::rest::Params params{
        {"zoom", "0.1,0.2,0.3x0.4"}};
    nx::core::transcoding::Settings settings;
    settings.layout = QnConstResourceVideoLayoutPtr(new TestVideoLayout());
    settings.zoomWindow = QRectF(QPointF{0.1, 0.2}, QSizeF{0.3, 0.4});
    checkResourceAndJson(params, settings, "");
}

TEST(TranscodingSettings, badZoomArgument)
{
    {
        nx::network::rest::Params params{
            {"zoom", "xxx"}};
        nx::core::transcoding::Settings settings;
        settings.layout = QnConstResourceVideoLayoutPtr(new TestVideoLayout());
        checkResourceAndJson(params, settings, "Invalid parameter 'zoom': \"xxx\"");
    }
    {
        nx::network::rest::Params params{
            {"zoom", "0.2,0.3x0.4"}};
        nx::core::transcoding::Settings settings;
        settings.layout = QnConstResourceVideoLayoutPtr(new TestVideoLayout());
        checkResourceAndJson(params, settings, "Invalid parameter 'zoom': \"0.2,0.3x0.4\"");
    }
    {
        nx::network::rest::Params params{
            {"zoom", "42,0.2,0.3x0.4"}};
        nx::core::transcoding::Settings settings;
        settings.layout = QnConstResourceVideoLayoutPtr(new TestVideoLayout());
        checkResourceAndJson(params, settings, "invalidZoom");
    }
}

TEST(TranscodingSettings, panoramicMatters)
{
    {
        nx::network::rest::Params params{
            {"panoramic", "true"}};
        nx::core::transcoding::Settings settings;
        settings.layout = QnConstResourceVideoLayoutPtr(new TestVideoLayout());
        checkResourceAndJson(params, settings, "");
    }
}

TEST(TranscodingSettings, badPanoramicArgument)
{
    nx::network::rest::Params params{
        {"panoramic", "xxx"}};
    nx::core::transcoding::Settings settings;
    settings.layout = QnConstResourceVideoLayoutPtr(new TestVideoLayout());
    checkResourceAndJson(params, settings, "Invalid parameter 'panoramic': \"xxx\"");
}
