// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <core/ptz/ptz_mapper.h>
#include <core/ptz/ptz_math.h>
#include <nx/fusion/model_functions.h>

namespace {

constexpr QByteArrayView kJson = R"json({
    "fromCamera": {
        "x": {
            "extrapolationMode": "PeriodicExtrapolation",
                "device" : [-18000, 18000],
                "logical" : [-180, 180]
        },
        "y" : {
            "extrapolationMode": "ConstantExtrapolation",
                "device" : [0, 9000],
                "logical" : [90, 0]
        },
        "z" : {
            "extrapolationMode": "ConstantExtrapolation",
                "device" : [
                    30, 80, 256, 336, 512, 592, 768, 848, 1024, 1104, 1280,
                    1360, 1536, 1616, 1792, 1872, 2048, 2128, 2304, 2384, 2520
                ],
                "logical" : [
                    1, 1.0319, 1.1540, 1.2209, 1.3808, 1.4651, 1.6802, 1.7936, 2.0959,
                    2.2587, 2.6660, 2.8696, 3.4621, 3.7953, 4.6840, 5.2209, 6.6650,
                    7.4982, 9.8495, 11.1825, 13.8485
                ],
                "logicalMultiplier" : 35.0,
                "space" : "35MmEquiv"
        }
    },
    "toCamera": {
        "z": {
            "extrapolationMode": "ConstantExtrapolation",
                "device" : [
                    0, 50, 100, 150, 200, 250, 300, 350, 400, 450, 500,
                    550, 600, 650, 700, 750, 800, 850, 900, 950, 1000
                ],
                "logical" : [
                    1, 1.0319, 1.1540, 1.2209, 1.3808, 1.4651, 1.6802, 1.7936, 2.0959, 2.2587,
                    2.6660, 2.8696, 3.4621, 3.7953, 4.6840, 5.2209, 6.6650, 7.4982, 9.8495,
                    11.1825, 13.8485
                ],
                "logicalMultiplier" : 35.0,
                "space" : "35MmEquiv"
        }
    }
})json";

struct TestCase
{
    nx::vms::common::ptz::Vector input;
    nx::vms::common::ptz::Vector output;
};

} // namespace

TEST(PtzMapper, deserialization)
{
    auto mapper = QJson::deserialized<QnPtzMapperPtr>(kJson);

    static const std::vector<TestCase> deviceToLogicalCases = {
        {{-18000, 0, 0, 30}, {-180, 90, 0, 54.4322}},
        {{-17000, 100, 0, 50}, {-170, 89, 0, 53.845}},
        {{-16000, 300, 0, 120}, {-160, 87, 0, 51.780}},
        {{-12500, 1200, 0, 180}, {-125, 78, 0, 50.066}},
        {{-10000, 2400, 0, 290}, {-100, 66, 0, 47.0136}},
        {{-5000, 3000, 0, 360}, {-50, 60, 0, 44.964}},
        {{-3700, 3650, 0, 410}, {-37, 53.5, 0, 43.530}},
        {{-900, 4060, 0, 440}, {-9, 49.4, 0, 42.710}},
        {{0, 4900, 0, 510}, {0, 41, 0, 40.906}},
        {{200, 5225, 0, 635}, {2, 37.75, 0, 37.440}},
        {{3895, 6800, 0, 715}, {38.95, 22, 0, 35.320}},
        {{6235, 6915, 0, 780}, {62.35, 20.85, 0, 33.717}},
        {{9000, 7100, 0, 805}, {90, 19, 0, 33.065}},
        {{12345, 7755, 0, 865}, {123.45, 12.45, 0, 31.514}},
        {{14785, 8010, 0, 910}, {147.85, 9.9, 0, 30.2916}},
        {{16000, 8235, 0, 960}, {160, 7.65, 0, 29.037}},
        {{18000, 9000, 0, 1000}, {180, 0, 0, 28.1133}}
    };

    static const std::vector<TestCase> logicalToDeviceCases = {
        {{-180, 90, 0, 54.4322f}, {-18000, 0, 0, 0.000749244}},
        {{-170, 89, 0, 1.0f}, {-17000, 100, 0, 1000}},
        {{-160, 87, 0, 1.2f}, {-16000, 300, 0, 1000}},
        {{-125, 78, 0, 1.7f}, {-12500, 1200, 0, 1000}},
        {{-100, 66, 0, 2.5f}, {-10000, 2400, 0, 1000}},
        {{-50, 60, 0, 3.8f}, {-5000, 3000, 0, 1000}},
        {{-37, 53.5f, 0, 4.8f}, {-3700, 3650, 0, 970.811}},
        {{-9, 49.4f, 0, 5.1f}, {-900, 4060, 0, 957.4244}},
        {{0, 41, 0, 5.4f}, {0, 4900, 0, 940.61}},
        {{2, 37.75f, 0, 5.7f}, {200, 5225, 0, 919.529}},
        {{38.95f, 22, 0, 6.9f}, {3895, 6800, 0, 872.1132}},
        {{62.35f, 20.85f, 0, 8.2f}, {6235, 6915, 0, 831.956}},
        {{90, 19, 0, 9.45f}, {9000, 7100, 0, 784.792}},
        {{123.45f, 12.45f, 0, 11.23f}, {12345, 7755, 0, 750.372}},
        {{147.85f, 9.9f, 0, 12.04f}, {14785, 8010, 0, 718.2236}},
        {{160, 7.65f, 0, 12.93f}, {16000, 8235, 0, 691.949}},
        {{180, 0, 0, 13.8485f}, {18000, 9000, 0, 674.740}}
    };

    for (const auto& testCase: deviceToLogicalCases)
    {
        const auto result = mapper->deviceToLogical(testCase.input);
        EXPECT_NEAR(result.pan, testCase.output.pan, 0.001);
        EXPECT_NEAR(result.tilt, testCase.output.tilt, 0.001);
        EXPECT_NEAR(result.zoom, testCase.output.zoom, 0.001);
    }

    for (const auto& testCase : logicalToDeviceCases)
    {
        const auto result = mapper->logicalToDevice(testCase.input);
        EXPECT_NEAR(result.pan, testCase.output.pan, 0.001);
        EXPECT_NEAR(result.tilt, testCase.output.tilt, 0.001);
        EXPECT_NEAR(result.zoom, testCase.output.zoom, 0.001);
    }
}

TEST(PtzMapper, Mapping)
{
    constexpr QByteArrayView json = R"json({
    "fromCamera": {
        "z" : {
            "extrapolationMode": "ConstantExtrapolation",
                "device" : [
                    0, 1
                ],
                "logical" : [
                    1, 10
                ],
                "logicalMultiplier" : 35.0,
                "space" : "35MmEquiv"
        }
    }
})json";

    using namespace nx::vms::common::ptz;
    auto mapper = QJson::deserialized<QnPtzMapperPtr>(json);

    const double minLogical = 35.;
    const double maxLogical = 350.;
    for (double x = 0; x <= 1.; x += 0.01)
    {
        EXPECT_NEAR(
            mapper->deviceToLogical(Vector{0,0,0,x}).zoom,
            q35mmEquivToDegrees(minLogical + (maxLogical - minLogical) * x),
            0.1);
    }

    for (double x = minLogical; x <= maxLogical; x += (maxLogical - minLogical) / 10.)
    {
        EXPECT_NEAR(
            mapper->logicalToDevice(Vector{0,0,0,q35mmEquivToDegrees(x)}).zoom,
            x / maxLogical,
            0.1);
    }
}
