#include <gtest/gtest.h>

#include <nx/fusion/model_functions.h>
#include <core/ptz/ptz_mapper.h>

namespace {

const auto kJson = QString::fromUtf8(R"json({
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
})json");

struct TestCase
{
    nx::core::ptz::Vector input;
    nx::core::ptz::Vector output;
};

} // namespace


TEST(PtzMapper, deserialization)
{
    auto mapper = QJson::deserialized<QnPtzMapperPtr>(kJson.toUtf8());

    static const std::vector<TestCase> deviceToLogicalCases = {
        {{-18000, 0, 0, 30}, {-180, 90, 0, 54.4322}},
        {{-17000, 100, 0, 50}, {-170, 89, 0, 53.8522}},
        {{-16000, 300, 0, 120}, {-160, 87, 0, 51.8591}},
        {{-12500, 1200, 0, 180}, {-125, 78, 0, 50.1745}},
        {{-10000, 2400, 0, 290}, {-100, 66, 0, 47.0396}},
        {{-5000, 3000, 0, 360}, {-50, 60, 0, 45.0266}},
        {{-3700, 3650, 0, 410}, {-37, 53.5, 0, 43.6547}},
        {{-900, 4060, 0, 440}, {-9, 49.4, 0, 42.8316}},
        {{0, 4900, 0, 510}, {0, 41, 0, 40.9111}},
        {{200, 5225, 0, 635}, {2, 37.75, 0, 37.5492}},
        {{3895, 6800, 0, 715}, {38.95, 22, 0, 35.4367}},
        {{6235, 6915, 0, 780}, {62.35, 20.85, 0, 33.7315}},
        {{9000, 7100, 0, 805}, {90, 19, 0, 33.0944}},
        {{12345, 7755, 0, 865}, {123.45, 12.45, 0, 31.5713}},
        {{14785, 8010, 0, 910}, {147.85, 9.9, 0, 30.4398}},
        {{16000, 8235, 0, 960}, {160, 7.65, 0, 29.1826}},
        {{18000, 9000, 0, 1000}, {180, 0, 0, 28.1768}}
    };

    static const std::vector<TestCase> logicalToDeviceCases = {
        {{-180, 90, 0, 54.4322f}, {-18000, 0, 0, 0.000749244}},
        {{-170, 89, 0, 1.0f}, {-17000, 100, 0, 1000}},
        {{-160, 87, 0, 1.2f}, {-16000, 300, 0, 1000}},
        {{-125, 78, 0, 1.7f}, {-12500, 1200, 0, 1000}},
        {{-100, 66, 0, 2.5f}, {-10000, 2400, 0, 1000}},
        {{-50, 60, 0, 3.8f}, {-5000, 3000, 0, 1000}},
        {{-37, 53.5f, 0, 4.8f}, {-3700, 3650, 0, 973.024}},
        {{-9, 49.4f, 0, 5.1f}, {-900, 4060, 0, 958.214}},
        {{0, 41, 0, 5.4f}, {0, 4900, 0, 940.61}},
        {{2, 37.75f, 0, 5.7f}, {200, 5225, 0, 919.529}},
        {{38.95f, 22, 0, 6.9f}, {3895, 6800, 0, 875.337}},
        {{62.35f, 20.85f, 0, 8.2f}, {6235, 6915, 0, 831.956}},
        {{90, 19, 0, 9.45f}, {9000, 7100, 0, 787.116}},
        {{123.45f, 12.45f, 0, 11.23f}, {12345, 7755, 0, 750.444}},
        {{147.85f, 9.9f, 0, 12.04f}, {14785, 8010, 0, 719.201}},
        {{160, 7.65f, 0, 12.93f}, {16000, 8235, 0, 693.135}},
        {{180, 0, 0, 13.8485f}, {18000, 9000, 0, 677.312}}
    };

    for (const auto& testCase: deviceToLogicalCases)
    {
        const auto result = mapper->deviceToLogical(testCase.input);

        // Data is not very precise, so ASSERT_DOUBLE_EQ is not the option.
        ASSERT_LT((result - testCase.output).length(), 0.001);
    }

    for (const auto& testCase : logicalToDeviceCases)
    {
        const auto result = mapper->logicalToDevice(testCase.input);
        ASSERT_LT((result - testCase.output).length(), 0.001);
    }
}
