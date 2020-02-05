#include <gtest/gtest.h>

#include <nx/utils/url.h>
#include <utils/camera/camera_diagnostics.h>

namespace nx::vms::server::test {

TEST(CameraDiagnostics, urlPasswordIsHidden)
{
    static const QString kUrlStr("http://test.com/path");
    static const QString kSimpleUrl("192.168.2.1");

    auto checkUrl = 
        [&](const CameraDiagnostics::Result& data)
        {
            ASSERT_FALSE(data.toString(nullptr).contains("password"));
            ASSERT_TRUE(data.toString(nullptr).contains(kUrlStr));
        };

    auto checkSimpleUrl =
        [&](const CameraDiagnostics::Result& data)
    {
        ASSERT_TRUE(data.toString(nullptr).contains(kSimpleUrl));
    };

    auto checkUrlHost =
        [&](const CameraDiagnostics::Result& data)
    {
        ASSERT_FALSE(data.toString(nullptr).contains("password"));
        ASSERT_TRUE(data.toString(nullptr).contains("test.com"));
    };

    nx::utils::Url url(kUrlStr);
    url.setUserName("admin");
    url.setPassword("password");

    checkUrl(CameraDiagnostics::CannotOpenCameraMediaPortResult(url, 0));
    checkUrlHost(CameraDiagnostics::ConnectionClosedUnexpectedlyResult(url.host(), 0));
    checkUrl(CameraDiagnostics::CameraResponseParseErrorResult(url, "test"));
    checkUrl(CameraDiagnostics::NoMediaTrackResult(url));
    checkUrl(CameraDiagnostics::NotAuthorisedResult(url));
    checkUrl(CameraDiagnostics::UnsupportedProtocolResult(url, "http"));

    checkSimpleUrl(CameraDiagnostics::CannotOpenCameraMediaPortResult(kSimpleUrl, 0));
    checkSimpleUrl(CameraDiagnostics::ConnectionClosedUnexpectedlyResult(kSimpleUrl, 0));
    checkSimpleUrl(CameraDiagnostics::CameraResponseParseErrorResult(kSimpleUrl, "test"));
    checkSimpleUrl(CameraDiagnostics::NoMediaTrackResult(kSimpleUrl));
    checkSimpleUrl(CameraDiagnostics::NotAuthorisedResult(kSimpleUrl));
    checkSimpleUrl(CameraDiagnostics::UnsupportedProtocolResult(kSimpleUrl, "http"));
}

} // namespace nx::vms::server::test
