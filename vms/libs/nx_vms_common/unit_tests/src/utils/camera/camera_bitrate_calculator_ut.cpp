// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <utils/camera/camera_bitrate_calculator.h>

namespace nx::vms::common::test {

TEST(CameraBitrateCalculatorTest, RealExamples)
{
    {
        const Qn::StreamQuality quality = Qn::StreamQuality::highest;
        const QSize resolution(1280, 960);
        const int fps = 25;
        const QString codec = "H264";

        // qualityFactor = 0.1 + 0.9*(4/4)
        // resolutionFactor = 0.009*(1280*960)^0.7
        // expected = (qualityFactor = 1.0) * (resolutionFactor = 164.77) * (frameRateFactor = 25.0) * (bitrateMultiplyer = 1.0)
        const float expected = std::round(4119.26074f);

        const float actual =
            CameraBitrateCalculator::suggestBitrateForQualityKbps(quality, resolution, fps, codec);

        ASSERT_FLOAT_EQ(actual, expected);
    }
    {
        const Qn::StreamQuality quality = Qn::StreamQuality::lowest;
        const QSize resolution(2560, 1440);
        const int fps = 1;
        const QString codec = "H264";

        // Expected value before max operation = 35.5520096.
        const float expected = 192.0f;

        const float actual =
            CameraBitrateCalculator::suggestBitrateForQualityKbps(quality, resolution, fps, codec);

        ASSERT_FLOAT_EQ(actual, expected);
    }
}

TEST(CameraBitrateCalculatorTest, BitrateDependenceOnInputParameters)
{
    const QSize baseResolution(1280, 720);
    const int baseFps = 25;
    const QString baseCodec = "H264";
    const Qn::StreamQuality baseQuality = Qn::StreamQuality::normal;

    // Quality
    {
        const std::vector<std::pair<Qn::StreamQuality, float>> paramsWithExpectedRes = {
            {Qn::StreamQuality::lowest,  337.0f},
            {Qn::StreamQuality::normal,  1852.0f},
            {Qn::StreamQuality::highest, 3368.0f}};

        for (const auto [param, expected] : paramsWithExpectedRes)
        {
            const float res = CameraBitrateCalculator::suggestBitrateForQualityKbps(
                param, baseResolution, baseFps, baseCodec);
            ASSERT_FLOAT_EQ(res, expected);
        }
    }
    // Resolution
    {
        const std::vector<std::pair<QSize, float>> paramsWithExpectedRes = {
            {QSize{0, 0},       3268.0f}, //< DefaultResolution will used
            {QSize{640, 360},   702.0f},
            {QSize{1280, 720},  1852.0f},
            {QSize{2560, 1440}, 4888.0f}};

        for (const auto [param, expected]: paramsWithExpectedRes)
        {
            const float res = CameraBitrateCalculator::suggestBitrateForQualityKbps(
                baseQuality, param, baseFps, baseCodec);
            ASSERT_FLOAT_EQ(res, expected);
        }
    }
    // fps
    {
        const std::vector<std::pair<int, float>> paramsWithExpectedRes = {
            {5,  370.0f},
            {15, 1111.0f},
            {30, 2223.0f}};

        for (const auto [param, expected]: paramsWithExpectedRes)
        {
            const float res = CameraBitrateCalculator::suggestBitrateForQualityKbps(
                baseQuality, baseResolution, param, baseCodec);
            ASSERT_FLOAT_EQ(res, expected);
        }
    }
    // Codec
    {
        const std::vector<std::pair<QString, float>> paramsWithExpectedRes = {
            {"H265",  1482.0f},
            {"H264",  1852.0f},
            {"MJPEG", 3705.0f}};

        for (const auto [param, expected]: paramsWithExpectedRes)
        {
            const float res = CameraBitrateCalculator::suggestBitrateForQualityKbps(
                baseQuality, baseResolution, baseFps, param);
            ASSERT_FLOAT_EQ(res, expected);
        }
    }
}

TEST(CameraBitrateCalculatorTest, UsesStreamCapability)
{
    nx::vms::api::CameraStreamCapability baseStreamCapability;
    baseStreamCapability.defaultBitrateKbps = 2000;
    baseStreamCapability.defaultFps = 25;
    baseStreamCapability.minBitrateKbps = 0;
    baseStreamCapability.maxBitrateKbps = 5000;

    const QSize baseResolution(1280, 720);
    const int baseFps = 25;
    const QString baseCodec = "H264";
    const Qn::StreamQuality baseQuality = Qn::StreamQuality::normal;

    // Quality
    {
        const std::vector<std::pair<Qn::StreamQuality, float>> paramsWithExpectedRes = {
            {Qn::StreamQuality::lowest, 660.0f},
            {Qn::StreamQuality::normal, 2000.0f},
            {Qn::StreamQuality::highest, 5000.0f}};//< maxBitrate

        for (const auto [param, expected]: paramsWithExpectedRes)
        {
            const float res = CameraBitrateCalculator::suggestBitrateForQualityKbps(
                param, baseResolution, baseFps, baseCodec, baseStreamCapability, false);
            ASSERT_FLOAT_EQ(res, expected);
        }
    }
    // Default bitrate
    {
        const std::vector<std::pair<float, float>> paramsWithExpectedRes = {
            {1000.0f, 1000.0f},
            {2000.0f, 2000.0f},
            {3000.0f, 3000.0f}};

        for (const auto [param, expected]: paramsWithExpectedRes)
        {
            nx::vms::api::CameraStreamCapability streamCapability = baseStreamCapability;
            streamCapability.defaultBitrateKbps = param;
            const float res = CameraBitrateCalculator::suggestBitrateForQualityKbps(
                baseQuality, baseResolution, baseFps, baseCodec, streamCapability, false);
            ASSERT_FLOAT_EQ(res, expected);
        }
    }
    // Default fps
    {
        const std::vector<std::pair<int, float>> paramsWithExpectedRes = {
            {5,  5000.0f}, //< maxBitrate
            {15, 3333.3333f},
            {30, 1666.6666f}};

        for (const auto [param, expected]: paramsWithExpectedRes)
        {
            nx::vms::api::CameraStreamCapability streamCapability = baseStreamCapability;
            streamCapability.defaultFps = param;
            const float res = CameraBitrateCalculator::suggestBitrateForQualityKbps(
                baseQuality, baseResolution, baseFps, baseCodec, streamCapability, false);
            ASSERT_FLOAT_EQ(res, expected);
        }
    }
    // fps
    {
        const std::vector<std::pair<int, float>> paramsWithExpectedRes = {
            {5, 400.0f},
            {15,1200.0f},
            {30,2400.0f}};

        for (const auto [param, expected]: paramsWithExpectedRes)
        {
            const float res = CameraBitrateCalculator::suggestBitrateForQualityKbps(
                baseQuality, baseResolution, param, baseCodec, baseStreamCapability, false);
            ASSERT_FLOAT_EQ(res, expected);
        }
    }
    // Resolution/codec
    {
        {
            const float res1 = CameraBitrateCalculator::suggestBitrateForQualityKbps(
                baseQuality, QSize{640, 360}, baseFps, baseCodec, baseStreamCapability, false);
            const float res2 = CameraBitrateCalculator::suggestBitrateForQualityKbps(
                baseQuality, QSize{2560, 1440}, baseFps, baseCodec, baseStreamCapability, false);
            ASSERT_FLOAT_EQ(res1, res2);
        }
        {
            const float res1 = CameraBitrateCalculator::suggestBitrateForQualityKbps(
                baseQuality, baseResolution, baseFps, "H264", baseStreamCapability, false);
            const float res2 = CameraBitrateCalculator::suggestBitrateForQualityKbps(
                baseQuality, baseResolution, baseFps, "MJPEG", baseStreamCapability, false);
            ASSERT_FLOAT_EQ(res1, res2);
        }
    }
}

TEST(CameraBitrateCalculatorTest, WhenDefaultBitrateIsZero)
{
    nx::vms::api::CameraStreamCapability baseStreamCapability;
    baseStreamCapability.defaultBitrateKbps = 0;
    baseStreamCapability.defaultFps = 0;

    {
        const float res1 = CameraBitrateCalculator::suggestBitrateForQualityKbps(
            Qn::StreamQuality::normal, QSize(1280, 720), 25, "H264", baseStreamCapability, false);
        const float res2 = CameraBitrateCalculator::suggestBitrateForQualityKbps(
            Qn::StreamQuality::normal, QSize(1280, 720), 25, "H264");
        ASSERT_FLOAT_EQ(res1, res2);
    }
    // useBitratePerGop
    {
        const float res1 = CameraBitrateCalculator::suggestBitrateForQualityKbps(
            Qn::StreamQuality::normal, QSize(1280, 720), 25, "H264", baseStreamCapability, false);
        const float res2 = CameraBitrateCalculator::suggestBitrateForQualityKbps(
            Qn::StreamQuality::normal, QSize(1280, 720), 25, "H264", baseStreamCapability, true);
        ASSERT_FLOAT_EQ(res1, 1852.0f);
        ASSERT_FLOAT_EQ(res2, 2222.40015f);
    }
}

} // namespace nx::vms::common::test
