// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtCore/QStringList>
#include <QtCore/QUrl>

#include <nx/vms/client/desktop/network/ping_utils.h>

namespace nx::vms::client::desktop {
namespace test {

TEST(PingUtilsTest, ipAddressesAndDnsNamesArePingable)
{
    const QStringList hosts{
        "192.168.0.10",
        "localhost",
        "NVR01",
        "camera-1.local",
        "cam_1.lan",
        "kamera.xn--mnchen-3ya.de",
        "::1",
        "fe80::1",
        "2001:db8::8a2e:370:7334",
        "::ffff:10.0.0.1",
        "fe80::1%12",
        "fe80::1%eth0",
        "fe80::1%eth0.100",
    };

    for (const auto& host: hosts)
        EXPECT_TRUE(isPingableHost(host)) << host.toStdString();
}

TEST(PingUtilsTest, hostsWithDisallowedCharactersAreNotPingable)
{
    const QStringList hosts{
        "",
        "v1.155.31.14.40&calc.exe&rem",
        "1.2.3.4&calc.exe",
        "fe80::1%&calc.exe&rem",
        "fe80::1%12&calc.exe&rem",
        "fe80::1%",
        "fe80::1%12%13",
        "camera%12",
        "%PATH%",
        "-n",
        "/n",
        "a b",
        "a,b",
        "a;b",
        "a=b",
        "a|b",
        "a^b",
        "a(b)",
        "a\"b",
        "a<b",
        "a>b",
        "$(id)",
        "`id`",
        "a'b",
        "a\\b",
        "a*b",
        "~a",
        "a\tb",
        "host\n",
        "kamera.m\u00fcnchen.de",
        "\uff11\uff12\uff17.0.0.1",
    };

    for (const auto& host: hosts)
        EXPECT_FALSE(isPingableHost(host)) << host.toStdString();
}

TEST(PingUtilsTest, hostsOfUrlsWithInjectedCommandAreNotPingable)
{
    const QStringList urls{
        "rtsp://[v1.155.31.14.40&calc.exe&rem]:554",
        "rtsp://[fe80::1%25&calc.exe&rem]:554",
    };

    for (const auto& url: urls)
    {
        const QString host = QUrl(url).host();
        ASSERT_FALSE(host.isEmpty()) << url.toStdString();
        EXPECT_FALSE(isPingableHost(host)) << url.toStdString();
    }
}

TEST(PingUtilsTest, hostsOfRegularUrlsArePingable)
{
    const QStringList urls{
        "rtsp://192.168.0.10:554/stream",
        "http://camera-1.local/onvif/device_service",
        "rtsp://[fe80::1]:554",
        "rtsp://[fe80::1%25eth0]:554",
    };

    for (const auto& url: urls)
        EXPECT_TRUE(isPingableHost(QUrl(url).host())) << url.toStdString();
}

} // namespace test
} // namespace nx::vms::client::desktop
