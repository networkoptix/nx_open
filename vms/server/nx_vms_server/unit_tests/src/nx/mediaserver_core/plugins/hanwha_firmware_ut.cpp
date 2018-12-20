#include <gtest/gtest.h>

#include <plugins/resource/hanwha/hanwha_firmware.h>

using namespace nx::vms::server::plugins;

TEST(HanwhaFirmware, parsing)
{
    static const std::map<QString, std::tuple<int, int, QString>> kFirmwareOptions = {
        {{"v0.12_123456"}, {0, 12, "123456"}},
        {{"2.10_897123987123987"}, {2, 10, "897123987123987"}},
        {{"v12.13"}, {12, 13, ""}},
        {{"v546"}, {546, 0, ""}},
        {{"120"}, {120, 0, ""}}
    };

    for (const auto& entry: kFirmwareOptions)
    {
        HanwhaFirmware firmware(entry.first);
        ASSERT_EQ(firmware.majorVersion(), std::get<0>(entry.second));
        ASSERT_EQ(firmware.minorVersion(), std::get<1>(entry.second));
        ASSERT_EQ(firmware.build(), std::get<2>(entry.second));
    }
}

TEST(HanwhaFirmware, comparsion)
{
    using namespace nx::vms::server::plugins;

    static const std::map<QString, QString> kEqualFirmwareOptions = {
        {{"v0.12_123456"}, {"0.12_123456"}},
        {{"2.10_897123987123987"}, {"v2.10_897123987123987"}},
        {{"v12.13_beta2"}, {"12.13.beta2"}}
    };

    for (const auto& entry: kEqualFirmwareOptions)
    {
        ASSERT_EQ(HanwhaFirmware(entry.first), HanwhaFirmware(entry.second));
        ASSERT_LE(HanwhaFirmware(entry.first), HanwhaFirmware(entry.second));
        ASSERT_GE(HanwhaFirmware(entry.first), HanwhaFirmware(entry.second));
        ASSERT_LE(HanwhaFirmware(entry.second), HanwhaFirmware(entry.first));
        ASSERT_GE(HanwhaFirmware(entry.second), HanwhaFirmware(entry.first));
    }

    static const std::map<QString, QString> kMoreThanFirmwareOptions = {
        {{"1.12"}, {"0.12_123456"}},
        {{"2.1_123"}, {"2.1"}},
        {{"10.234_beta1"}, {"2.689"}},
        {{"2.11_test"}, {"v2.2_test"}}
    };

    for (const auto& entry: kMoreThanFirmwareOptions)
    {
        ASSERT_GT(HanwhaFirmware(entry.first), HanwhaFirmware(entry.second));
        ASSERT_LT(HanwhaFirmware(entry.second), HanwhaFirmware(entry.first));
    }
}
