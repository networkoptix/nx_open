/**********************************************************
* Dec 27, 2016
* rvasilenko
***********************************************************/
#include <QFile>
#include <vector>

#include <gtest/gtest.h>
#include <server/server_globals.h>
#include "utils.h"
#include "mediaserver_launcher.h"
#include <api/app_server_connection.h>
#include <nx/network/http/httpclient.h>

TEST(AuthWhileRestarting, CloudUserAuthWhileRestarting)
{
    MediaServerLauncher mediaServerLauncher;
    ASSERT_TRUE(mediaServerLauncher.start());

    ec2::ApiUserData userData;
    userData.id = QnUuid::createUuid();
    userData.name = "Vasja pupkin@gmail.com";
    userData.email = userData.name;
    userData.isCloud = true;
    {
        auto ec2Connection = QnAppServerConnectionFactory::getConnection2();
        ec2::AbstractUserManagerPtr userManager = ec2Connection->getUserManager(Qn::kSystemAccess);
        ASSERT_EQ(userManager->saveSync(userData), ec2::ErrorCode::ok);
    }
    mediaServerLauncher.stop();

    mediaServerLauncher.startAsync();

    //waiting for server to come up

    nx_http::HttpClient httpClient;
    httpClient.setUserName(userData.name);
    httpClient.setUserPassword("invalid password");
    httpClient.setAuthType(nx_http::AsyncHttpClient::authBasic);
    const auto startTime = std::chrono::steady_clock::now();
    constexpr const auto maxPeriodToWaitForMediaServerStart = std::chrono::seconds(150);
    while (std::chrono::steady_clock::now() - startTime < maxPeriodToWaitForMediaServerStart)
    {
        QUrl url = mediaServerLauncher.apiUrl();
        url.setPath("/ec2/getUsers");
        if (httpClient.doGet(url))
            break;  //server is alive
    }
    ASSERT_TRUE(httpClient.response());
    auto statusCode = httpClient.response()->statusLine.statusCode;
    ASSERT_EQ(statusCode, nx_http::StatusCode::forbidden);
}
