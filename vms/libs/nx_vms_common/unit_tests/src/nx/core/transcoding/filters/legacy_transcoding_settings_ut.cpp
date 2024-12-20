// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <core/resource/media_resource.h>
#include <core/resource/resource_media_layout.h>
#include <nx/core/transcoding/filters/legacy_transcoding_settings.h>
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

class TestMediaResource: public QnMediaResource
{
public:
    virtual ~TestMediaResource() override {}
    virtual bool hasVideo([[maybe_unused]] const QnAbstractStreamDataProvider* dataProvider = nullptr) const override { return true; }

    virtual QnConstResourceVideoLayoutPtr getVideoLayout(const QnAbstractStreamDataProvider* dataProvider = nullptr) override
    {
        if (videoLayout)
            return videoLayout;
        return QnMediaResource::getVideoLayout(dataProvider);
    }

    QnResourceVideoLayoutPtr videoLayout;
};

void isEqual(const QnLegacyTranscodingSettings& left, const QnLegacyTranscodingSettings& right)
{
    EXPECT_EQ(left.resource.get(), right.resource.get());
    EXPECT_EQ(left.forcedAspectRatio, right.forcedAspectRatio);
    EXPECT_EQ(left.rotation, right.rotation);
    EXPECT_EQ(left.zoomWindow, right.zoomWindow);
    EXPECT_EQ(left.contrastParams, right.contrastParams);
    EXPECT_EQ(left.forceDewarping, right.forceDewarping);
    EXPECT_EQ(left.itemDewarpingParams, right.itemDewarpingParams);
    EXPECT_EQ(left.timestampParams, right.timestampParams);
    EXPECT_EQ(left.cameraNameParams, right.cameraNameParams);
    EXPECT_EQ(left.watermark, right.watermark);
    EXPECT_EQ(left.panoramic, right.panoramic);
    EXPECT_EQ(left.isEmpty(), right.isEmpty());
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
    QnMediaResourcePtr resource,
    const nx::network::rest::Params& params,
    const QnLegacyTranscodingSettings& expected,
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
    QnLegacyTranscodingSettings legacySettings(resource, &settings);
    isEqual(legacySettings, expected);
}

TEST(LegacyTranscodingSettings, emptyParams)
{
    nx::network::rest::Params params{};
    QnMediaResourcePtr resource(new TestMediaResource());
    QnLegacyTranscodingSettings settings;
    settings.resource = resource;
    checkResourceAndJson(resource, params, settings, "");
}

TEST(LegacyTranscodingSettings, resolutionDontMatters)
{
    nx::network::rest::Params params{
        {"resolution", "640x480"}};
    QnMediaResourcePtr resource(new TestMediaResource());
    QnLegacyTranscodingSettings settings;
    settings.resource = resource;
    checkResourceAndJson(resource, params, settings, "");
}

TEST(LegacyTranscodingSettings, badResolutionArgument)
{
    nx::network::rest::Params params{
        {"resolution", "xxx"}};
    QnMediaResourcePtr resource(new TestMediaResource());
    QnLegacyTranscodingSettings settings;
    settings.resource = resource;
    checkResourceAndJson(resource, params, settings, "Invalid parameter 'resolution': \"xxx\"");
}

TEST(LegacyTranscodingSettings, resolutionThenTranscodingDontMatters)
{
    nx::network::rest::Params params{
        {"resolutionWhenTranscoding", "640x480"}};
    QnMediaResourcePtr resource(new TestMediaResource());
    QnLegacyTranscodingSettings settings;
    settings.resource = resource;
    checkResourceAndJson(resource, params, settings, "");
}

TEST(LegacyTranscodingSettings, badResolutionThenTranscodingArgument)
{
    nx::network::rest::Params params{
        {"resolutionThenTranscoding", "xxx"}};
    QnMediaResourcePtr resource(new TestMediaResource());
    QnLegacyTranscodingSettings settings;
    settings.resource = resource;
    checkResourceAndJson(resource, params, settings, "Invalid parameter 'resolutionThenTranscoding': \"xxx\"");
}

TEST(LegacyTranscodingSettings, streamNotMatters)
{
    {
        nx::network::rest::Params params{
            {"stream", "0"}};
        QnMediaResourcePtr resource(new TestMediaResource());
        QnLegacyTranscodingSettings settings;
        settings.resource = resource;
        checkResourceAndJson(resource, params, settings, "");
    }
    {
        nx::network::rest::Params params{
            {"stream", "1"}};
        QnMediaResourcePtr resource(new TestMediaResource());
        QnLegacyTranscodingSettings settings;
        settings.resource = resource;
        checkResourceAndJson(resource, params, settings, "");
    }
    {
        nx::network::rest::Params params{
            {"stream", "-1"}};
        QnMediaResourcePtr resource(new TestMediaResource());
        QnLegacyTranscodingSettings settings;
        settings.resource = resource;
        checkResourceAndJson(resource, params, settings, "");
    }
    {
        nx::network::rest::Params params{
            {"stream", "primary"}};
        QnMediaResourcePtr resource(new TestMediaResource());
        QnLegacyTranscodingSettings settings;
        settings.resource = resource;
        checkResourceAndJson(resource, params, settings, "");
    }
    {
        nx::network::rest::Params params{
            {"stream", "secondary"}};
        QnMediaResourcePtr resource(new TestMediaResource());
        QnLegacyTranscodingSettings settings;
        settings.resource = resource;
        checkResourceAndJson(resource, params, settings, "");
    }
    {
        nx::network::rest::Params params{
            {"stream", ""}};
        QnMediaResourcePtr resource(new TestMediaResource());
        QnLegacyTranscodingSettings settings;
        settings.resource = resource;
        checkResourceAndJson(resource, params, settings, "");
    }
}

TEST(LegacyTranscodingSettings, streamInvalid)
{
    {
        nx::network::rest::Params params{
            {"stream", "invalid"}};
        QnMediaResourcePtr resource(new TestMediaResource());
        QnLegacyTranscodingSettings settings;
        settings.resource = resource;
        checkResourceAndJson(resource, params, settings, "Invalid parameter 'stream': \"invalid\"");
    }
    {
        nx::network::rest::Params params{
            {"stream", "2"}};
        QnMediaResourcePtr resource(new TestMediaResource());
        QnLegacyTranscodingSettings settings;
        settings.resource = resource;
        checkResourceAndJson(resource, params, settings, "invalidStreamIndex");
    }
}

TEST(LegacyTranscodingSettings, rotationMatters)
{
    {
        nx::network::rest::Params params{
            {"rotation", "0"}};
        QnMediaResourcePtr resource(new TestMediaResource());
        QnLegacyTranscodingSettings settings;
        settings.resource = resource;
        checkResourceAndJson(resource, params, settings, "");
    }
    {
        nx::network::rest::Params params{
            {"rotation", "90"}};
        QnMediaResourcePtr resource(new TestMediaResource());
        QnLegacyTranscodingSettings settings;
        settings.resource = resource;
        settings.rotation = 90;
        checkResourceAndJson(resource, params, settings, "");
    }
    {
        nx::network::rest::Params params{
            {"rotation", "auto"}};
        QnMediaResourcePtr resource(new TestMediaResource());
        QnLegacyTranscodingSettings settings;
        settings.resource = resource;
        checkResourceAndJson(resource, params, settings, "");
    }
    {
        nx::network::rest::Params params{
            {"rotation", "auto"}};
        QnMediaResourcePtr resource(new TestMediaResource());
        resource->setForcedRotation(90);
        QnLegacyTranscodingSettings settings;
        settings.resource = resource;
        settings.rotation = 90;
        checkResourceAndJson(resource, params, settings, "");
    }
    {
        nx::network::rest::Params params{
            {"rotation", "90"}};
        QnMediaResourcePtr resource(new TestMediaResource());
        resource->setForcedRotation(-90);
        QnLegacyTranscodingSettings settings;
        settings.resource = resource;
        settings.rotation = 90;
        checkResourceAndJson(resource, params, settings, "");
    }
}

TEST(LegacyTranscodingSettings, badRotationArgument)
{
    nx::network::rest::Params params{
        {"rotation", "-90"}};
    QnMediaResourcePtr resource(new TestMediaResource());
    QnLegacyTranscodingSettings settings;
    settings.resource = resource;
    checkResourceAndJson(resource, params, settings, "invalidRotation");
}

TEST(LegacyTranscodingSettings, aspectRatioMatters)
{
    {
        nx::network::rest::Params params{
            {"aspectRatio", "0:0"}};
        QnMediaResourcePtr resource(new TestMediaResource());
        QnLegacyTranscodingSettings settings;
        settings.resource = resource;
        checkResourceAndJson(resource, params, settings, "");
    }
    {
        nx::network::rest::Params params{
            {"aspectRatio", "4:3"}};
        QnMediaResourcePtr resource(new TestMediaResource());
        QnLegacyTranscodingSettings settings;
        settings.resource = resource;
        settings.forcedAspectRatio = QnAspectRatio(4, 3);
        checkResourceAndJson(resource, params, settings, "");
    }
    {
        nx::network::rest::Params params{
            {"aspectRatio", "auto"}};
        QnMediaResourcePtr resource(new TestMediaResource());
        QnLegacyTranscodingSettings settings;
        settings.resource = resource;
        checkResourceAndJson(resource, params, settings, "");
    }
    {
        nx::network::rest::Params params{
            {"aspectRatio", "auto"}};
        QnMediaResourcePtr resource(new TestMediaResource());
        resource->setCustomAspectRatio(QnAspectRatio(4, 3));
        QnLegacyTranscodingSettings settings;
        settings.resource = resource;
        settings.forcedAspectRatio = QnAspectRatio(4, 3);
        checkResourceAndJson(resource, params, settings, "");
    }
    {
        nx::network::rest::Params params{
            {"aspectRatio", "4:3"}};
        QnMediaResourcePtr resource(new TestMediaResource());
        resource->setCustomAspectRatio(QnAspectRatio(16, 9));
        QnLegacyTranscodingSettings settings;
        settings.resource = resource;
        settings.forcedAspectRatio = QnAspectRatio(4, 3);
        checkResourceAndJson(resource, params, settings, "");
    }
}

TEST(LegacyTranscodingSettings, badAspectRatioArgument)
{
    {
        nx::network::rest::Params params{
            {"aspectRatio", "-90"}};
        QnMediaResourcePtr resource(new TestMediaResource());
        QnLegacyTranscodingSettings settings;
        settings.resource = resource;
        checkResourceAndJson(resource, params, settings, "Invalid parameter 'aspectRatio': \"-90\"");
    }
    {
        nx::network::rest::Params params{
            {"aspectRatio", "x:y"}};
        QnMediaResourcePtr resource(new TestMediaResource());
        QnLegacyTranscodingSettings settings;
        settings.resource = resource;
        checkResourceAndJson(resource, params, settings, "Invalid parameter 'aspectRatio': \"x:y\"");
    }
}

TEST(LegacyTranscodingSettings, dewarpingMatters)
{
    {
        nx::network::rest::Params params{
            {"dewarping", "0"}};
        QnMediaResourcePtr resource(new TestMediaResource());
        QnLegacyTranscodingSettings settings;
        settings.resource = resource;
        checkResourceAndJson(resource, params, settings, "");
    }
    {
        nx::network::rest::Params params{
            {"dewarping", "1"}};
        QnMediaResourcePtr resource(new TestMediaResource());
        QnLegacyTranscodingSettings settings;
        settings.resource = resource;
        settings.forceDewarping = true;
        settings.itemDewarpingParams.enabled = true;
        checkResourceAndJson(resource, params, settings, "");
    }
    {
        nx::network::rest::Params params{
            {"dewarping", "true"},
            {"dewarpingXangle", "0.5"},
            {"dewarpingYangle", "0.3"},
            {"dewarpingFov", "0.4"},
            {"dewarpingPanofactor", "2"}};
        QnMediaResourcePtr resource(new TestMediaResource());
        QnLegacyTranscodingSettings settings;
        settings.resource = resource;
        settings.forceDewarping = true;
        settings.itemDewarpingParams.enabled = true;
        settings.itemDewarpingParams.xAngle = 0.5;
        settings.itemDewarpingParams.yAngle = 0.3;
        settings.itemDewarpingParams.fov = 0.4;
        settings.itemDewarpingParams.panoFactor = 2;
        checkResourceAndJson(resource, params, settings, "");
    }
}

TEST(LegacyTranscodingSettings, badDewarpingArgument)
{
    {
        nx::network::rest::Params params{
            {"dewarping", "42"}};
        QnMediaResourcePtr resource(new TestMediaResource());
        QnLegacyTranscodingSettings settings;
        settings.resource = resource;
        checkResourceAndJson(resource, params, settings, "Invalid parameter 'dewarping': \"42\"");
    }
    {
        nx::network::rest::Params params{
            {"dewarping", "true"},
            {"dewarpingXangle", "0.5"},
            {"dewarpingYangle", "0.3"},
            {"dewarpingFov", "0.4"},
            {"dewarpingPanofactor", "3"}};
        QnMediaResourcePtr resource(new TestMediaResource());
        QnLegacyTranscodingSettings settings;
        settings.resource = resource;
        checkResourceAndJson(resource, params, settings, "invalidDewarpingPanofactor");
    }
    {
        nx::network::rest::Params params{
            {"dewarping", "true"},
            {"dewarpingXangle", "xxx"},
            {"dewarpingYangle", "0.3"},
            {"dewarpingFov", "0.4"},
            {"dewarpingPanofactor", "2"}};
        QnMediaResourcePtr resource(new TestMediaResource());
        QnLegacyTranscodingSettings settings;
        settings.resource = resource;
        checkResourceAndJson(resource, params, settings, "Invalid parameter 'dewarpingXangle': \"xxx\"");
    }
    {
        nx::network::rest::Params params{
            {"dewarping", "true"},
            {"dewarpingXangle", "0.5"},
            {"dewarpingYangle", "xxx"},
            {"dewarpingFov", "0.4"},
            {"dewarpingPanofactor", "2"}};
        QnMediaResourcePtr resource(new TestMediaResource());
        QnLegacyTranscodingSettings settings;
        settings.resource = resource;
        checkResourceAndJson(resource, params, settings, "Invalid parameter 'dewarpingYangle': \"xxx\"");
    }
    {
        nx::network::rest::Params params{
            {"dewarping", "true"},
            {"dewarpingXangle", "0.5"},
            {"dewarpingYangle", "0.3"},
            {"dewarpingFov", "xxx"},
            {"dewarpingPanofactor", "2"}};
        QnMediaResourcePtr resource(new TestMediaResource());
        QnLegacyTranscodingSettings settings;
        settings.resource = resource;
        checkResourceAndJson(resource, params, settings, "Invalid parameter 'dewarpingFov': \"xxx\"");
    }
}

TEST(LegacyTranscodingSettings, zoomMatters)
{
    nx::network::rest::Params params{
        {"zoom", "0.1,0.2,0.3x0.4"}};
    QnMediaResourcePtr resource(new TestMediaResource());
    QnLegacyTranscodingSettings settings;
    settings.resource = resource;
    settings.zoomWindow = QRectF(QPointF{0.1, 0.2}, QSizeF{0.3, 0.4});
    checkResourceAndJson(resource, params, settings, "");
}

TEST(LegacyTranscodingSettings, badZoomArgument)
{
    {
        nx::network::rest::Params params{
            {"zoom", "xxx"}};
        QnMediaResourcePtr resource(new TestMediaResource());
        QnLegacyTranscodingSettings settings;
        settings.resource = resource;
        checkResourceAndJson(resource, params, settings, "Invalid parameter 'zoom': \"xxx\"");
    }
    {
        nx::network::rest::Params params{
            {"zoom", "0.2,0.3x0.4"}};
        QnMediaResourcePtr resource(new TestMediaResource());
        QnLegacyTranscodingSettings settings;
        settings.resource = resource;
        checkResourceAndJson(resource, params, settings, "Invalid parameter 'zoom': \"0.2,0.3x0.4\"");
    }
    {
        nx::network::rest::Params params{
            {"zoom", "42,0.2,0.3x0.4"}};
        QnMediaResourcePtr resource(new TestMediaResource());
        QnLegacyTranscodingSettings settings;
        settings.resource = resource;
        checkResourceAndJson(resource, params, settings, "invalidZoom");
    }
}

TEST(LegacyTranscodingSettings, panoramicMatters)
{
    {
        nx::network::rest::Params params{
            {"panoramic", "true"}};
        QnMediaResourcePtr resource(new TestMediaResource());
        QnLegacyTranscodingSettings settings;
        settings.resource = resource;
        checkResourceAndJson(resource, params, settings, "");
    }
    {
        nx::network::rest::Params params{
            {"panoramic", "true"}};
        auto testResource = new TestMediaResource();
        QnMediaResourcePtr resource(testResource);
        auto testVideoLayout = new TestVideoLayout();
        testResource->videoLayout = QnResourceVideoLayoutPtr(testVideoLayout);
        testVideoLayout->videoChannelCount = 2;
        QnLegacyTranscodingSettings settings;
        settings.resource = resource;
        settings.panoramic = true;
        checkResourceAndJson(resource, params, settings, "");
    }
}

TEST(LegacyTranscodingSettings, badPanoramicArgument)
{
    nx::network::rest::Params params{
        {"panoramic", "xxx"}};
    QnMediaResourcePtr resource(new TestMediaResource());
    QnLegacyTranscodingSettings settings;
    settings.resource = resource;
    checkResourceAndJson(resource, params, settings, "Invalid parameter 'panoramic': \"xxx\"");
}
