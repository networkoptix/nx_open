// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/vms/client/desktop/manual_device_addition/dialog/private/add_device_address_parser.h>

using namespace nx::vms::client::desktop::manual_device_addition;

namespace nx::vms::client::desktop::test {

struct AddDeviceAddressParserTestCase
{
    const char* input;
    AddDeviceSearchAddress result;
} static const kTestCases[] = {
    // input / start address, end address, port, port index, port length
    { "www.example.com:8080", { "www.example.com:8080", {}, 8080, 16, 4 }, },
    { "http://example.com:7090", { "http://example.com:7090", {}, 7090, 19, 4 }, },
    { "rtsp://example.com:554/video", { "rtsp://example.com:554/video", {}, 554, 19, 3 }, },
    { "udp://239.250.5.5:1234", { "udp://239.250.5.5", {}, 1234, 18, 4 }, },
    { "udp://239.250.5.5:1234-239.250.5.255", { "udp://239.250.5.5", "udp://239.250.5.255", 1234, 18, 4 }, },
    { "udp://239.250.5.5:1234-239.250.5.255:5678", { "udp://239.250.5.5", "udp://239.250.5.255", 1234, 18, 4 }, },
    { "192.168.1.1-192.168.1.20", { "192.168.1.1", "192.168.1.20", {}, -1, -1 }, },
    { "192.168.1.1-192.168.1.20", { "192.168.1.1", "192.168.1.20", {}, -1, -1 }, },
    { "192.168.1.1--192.168.1.20 ", { "192.168.1.1", "192.168.1.20", {}, -1, -1 }, },
    { "192.168.1.1 -192.168.1.20", { "192.168.1.1", "192.168.1.20", {}, -1, -1 }, },
    { "192.168.1.1- 192.168.1.20", { "192.168.1.1", "192.168.1.20", {}, -1, -1 }, },
    { "192.168.1.1 -- 192.168.1.20", { "192.168.1.1", "192.168.1.20", {}, -1, -1 }, },
    { "192.168.1.1-- 192.168.1.20", { "192.168.1.1", "192.168.1.20", {}, -1, -1 }, },
    { "192.168.1.1 --192.168.1.20", { "192.168.1.1", "192.168.1.20", {}, -1, -1 }, },
    { "192.168.1.1 - 192.168.1.20", { "192.168.1.1", "192.168.1.20", {}, -1, -1 }, },
    { "192.168.1.1:99 --- 192.168.1.20:156", { "192.168.1.1", "192.168.1.20", 99, 12, 2 }, },
    { "192.168.1.1/24", { "192.168.1.0", "192.168.1.255", {}, -1, -1 }, },
    { "192.168.1.1:8657/24", { "192.168.1.0", "192.168.1.255", 8657, 12, 4 }, },
    { "192.168.1.1/24:9876", { "192.168.1.0", "192.168.1.255", 9876, 15, 4 }, },
    { "192.168.1.1/16", { "192.168.0.0", "192.168.255.255", {}, -1, -1 }, },
    { "192.168.1.128/25", { "192.168.1.128", "192.168.1.255", {}, -1, -1 }, },
    { "10.0.0.1/25", { "10.0.0.0", "10.0.0.127", {}, -1, -1 }, },
    { "204.204.125.252/28", { "204.204.125.240", "204.204.125.255", {}, -1, -1 }, },
    { "localhost", { "localhost", {}, {}, -1, -1 }, },
    { "localhost:9987", { "localhost:9987", {}, 9987, 10, 4 }, },
    { "1.2.345.6- 10.20.30.99:456", { "1.2.345.6", "10.20.30.99", {}, -1, -1 }, },
    { "www.yandex.ru:9999999", { "www.yandex.ru:9999999" }, },   // port out of range
    { "121.10.9", { "121.10.9" }, },    // invalid address
    { "", { "" }, },
};

TEST(AddDeviceAddressParserTest, extractAddressAndPort)
{
    for (const auto& [input, expected]: kTestCases)
    {
        auto actual = parseDeviceAddress(input);
        ASSERT_EQ(actual.startAddress, expected.startAddress);
        ASSERT_EQ(actual.endAddress, expected.endAddress);
        ASSERT_EQ(actual.port, expected.port);
        ASSERT_EQ(actual.portIndex, expected.portIndex);
        ASSERT_EQ(actual.portLength, expected.portLength);
    }
}

} // namespace nx::vms::client::desktop::test
