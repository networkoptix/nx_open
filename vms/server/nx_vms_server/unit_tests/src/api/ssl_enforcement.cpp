#include <gtest/gtest.h>

#include <nx/utils/log/log.h>
#include <core/resource/camera_bookmark.h>

#include "test_api_requests.h"

namespace nx {
namespace test {

using namespace api_requests_detail;

TEST(SslEnforcement, main)
{
    MediaServerLauncher launcher;
    ASSERT_TRUE(launcher.start());
    const auto request =
        [&](QString path)
        {
              auto client = createHttpClient();
              EXPECT_TRUE(client->doGet(createUrl(&launcher, path)))<< path.toStdString();
              EXPECT_TRUE(client->response()
                    && client->response()->statusLine.statusCode == 200) << path.toStdString();
              return client;
        };

    // Pure HTTP is allowed by default.
    ASSERT_EQ(network::http::kUrlSchemeName,
        request("/api/moduleInformation")->contentLocationUrl().scheme().toUtf8());

    request("/api/systemSettings?trafficEncryptionForced=1");

    // Authomatic redirect is supposed to happen.
    ASSERT_EQ(network::http::kSecureUrlSchemeName,
        request("/api/moduleInformation")->contentLocationUrl().scheme().toUtf8());
}

} // namespace test
} // namespace nx
