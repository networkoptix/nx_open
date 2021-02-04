#include <gtest/gtest.h>

#include <test_support/mediaserver_launcher.h>
#include <nx/mediaserver/camera_mock.h>
#include <media_server/media_server_module.h>
#include <core/resource_management/resource_pool.h>
#include <nx/network/http/http_client.h>
#include <nx/network/http/custom_headers.h>

TEST(CyclicProxy, ResourceWithServerUrl)
{
    MediaServerLauncher launcher;
    ASSERT_TRUE(launcher.start());
    QnSharedResourcePointer<resource::test::CameraMock> camera(
        new resource::test::CameraMock(launcher.serverModule(), 1));
    camera->setUrl(launcher.apiUrl().toString());
    launcher.commonModule()->resourcePool()->addResource(camera);
    nx::utils::Url requestUrl = launcher.apiUrl();
    requestUrl.setPath("/ec2/getCameras");
    nx::network::http::HttpClient httpClient;
    httpClient.setUserName("admin");
    httpClient.setUserPassword("admin");
    httpClient.setProxyUserName("admin");
    httpClient.setProxyUserPassword("admin");
    ASSERT_TRUE(httpClient.doGet(requestUrl));
    httpClient.addAdditionalHeader(Qn::CAMERA_GUID_HEADER_NAME, camera->getId().toString().toUtf8());
    httpClient.doGet(requestUrl); // Cycled proxy should close connection on zero TTL
}
