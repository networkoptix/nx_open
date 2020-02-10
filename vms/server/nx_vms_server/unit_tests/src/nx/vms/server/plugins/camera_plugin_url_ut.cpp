#include <gtest/gtest.h>

#include <QtCore/QString>

#include <plugins/camera_plugin_qt_wrapper.h>

namespace nx::vms::server::plugins::test {

TEST(CameraDiscoveryManager, urlConversion)
{
    using namespace nxcip_qt;
    auto checkUrl = 
        [](const QString& urlStr)
        {
            nx::utils::Url url(urlStr);
            ASSERT_EQ(urlStr.toStdString(), CameraDiscoveryManager::urlToString(url).toStdString());
            url = nx::utils::url::parseUrlFields(urlStr);
            ASSERT_EQ(urlStr.toStdString(), CameraDiscoveryManager::urlToString(url).toStdString());
        };

    checkUrl("rtsp://test.com:550/path?param1=2");
    checkUrl("192.168.4.1");
    checkUrl("d:/test/video");
    checkUrl("/user/local");
}

} // nx::vms::server::plugins::test
