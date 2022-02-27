// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <thread>

#include <gtest/gtest.h>

#include <QtTest/QSignalSpy>

#include <QtCore/QScopedPointer>
#include <QtQml/QQmlEngine>

#include <QtHttpServer.h>
#include <QtHttpRequest.h>
#include <QtHttpReply.h>

#include <client/client_module.h>
#include <nx/vms/client/desktop/resource_properties/camera/utils/camera_web_page_workarounds.h>
#include <nx/vms/client/desktop/common/widgets/webview_widget.h>
#include <nx/vms/client/desktop/utils/webengine_profile_manager.h>

using namespace std::chrono;

namespace nx::vms::client::desktop {

namespace test {

namespace {

constexpr milliseconds kPageWaitLimit = 10000ms;
constexpr quint16 kPortRangeBegin = 10000;
constexpr quint16 kPortRangeEnd = 65535;

} // namespace

class CameraWebPageWorkaroundsTest: public testing::Test
{
protected:
    // This function is named SetUpTestSuite() in GTest 1.10+
    static void SetUpTestCase()
    {
        QnClientModule::initWebEngine();
        qRegisterMetaType<QtHttpRequest*>();
        qRegisterMetaType<QtHttpReply*>();
        utils::WebEngineProfileManager::registerQmlType();
    }

    CameraWebPageWorkaroundsTest():
        m_engine(new QQmlEngine()),
        m_view(new WebViewWidget(nullptr, m_engine.data()))
    {
    }

    void givenWebServerWithHtml(const QByteArray& html, milliseconds responseDelay = 0ms)
    {
        m_server.reset(new QtHttpServer());

        for (m_port = kPortRangeBegin; m_port < kPortRangeEnd; ++m_port)
        {
            if (m_server->start(m_port, QHostAddress::LocalHost))
                break; //< Success.
        }

        ASSERT_NE(kPortRangeEnd, m_port);

        QObject::connect(m_server.get(), &QtHttpServer::requestNeedsReply,
            [html, responseDelay, this](QtHttpRequest* request, QtHttpReply* reply)
            {
                const auto path = request->getUrl().path();
                m_pathRequestCount[path] = m_pathRequestCount.value(path, 0) + 1;
                if (path == "/")
                {
                    reply->setStatusCode(QtHttpReply::Ok);
                    reply->addHeader("Content-Type", QByteArrayLiteral ("text/html"));
                    reply->appendRawData(html);
                }
                else if (path == "/wait" && responseDelay != 0ms)
                {
                    std::this_thread::sleep_for(responseDelay);
                }
            });
    }

    void whenPageLoaded(int expectedRequestCount)
    {
        QSignalSpy loadSpy(controller(), &WebViewController::loadFinished);
        QSignalSpy requestSpy(m_server.get(), &QtHttpServer::requestNeedsReply);

        controller()->load(webServerUrl());

        // Wait for loadFinished() finished
        loadSpy.wait(kPageWaitLimit.count());

        ASSERT_EQ(1, loadSpy.count());
        ASSERT_TRUE(loadSpy.takeFirst().at(0).toBool());

        // Wait for page requests to load.
        while (expectedRequestCount - requestSpy.count() > 0)
            ASSERT_TRUE(requestSpy.wait(kPageWaitLimit.count()));

        ASSERT_EQ(expectedRequestCount, requestSpy.count());
    }

    QUrl webServerUrl() const
    {
        QUrl url("http://127.0.0.1/");
        url.setPort(m_port);
        return url;
    }

    WebViewController* controller()
    {
        return m_view->controller();
    }

    int pathRequestCount(const QString& path) const
    {
        return m_pathRequestCount.value(path, 0);
    }

private:
    QScopedPointer<QQmlEngine> m_engine;
    QScopedPointer<WebViewWidget> m_view;
    QScopedPointer<QtHttpServer> m_server;
    QMap<QString, int> m_pathRequestCount;
    quint16 m_port = 0;
};

TEST_F(CameraWebPageWorkaroundsTest, setXmlHttpRequestTimeout)
{
    static constexpr milliseconds kRequestDelay = 1000ms;
    givenWebServerWithHtml(R"HTML(
        <html>
            <script>
                var xhr = new XMLHttpRequest();

                xhr.open('GET', '/wait', true);
                xhr.timeout = 1;

                xhr.onload = function (e) {
                    var xhr2 = new XMLHttpRequest();
                    xhr2.open('GET', '/success', true);
                    xhr2.send();
                };
                xhr.send();
            </script>
        </html>
        )HTML", kRequestDelay);

    CameraWebPageWorkarounds::setXmlHttpRequestTimeout(controller(), 2 * kRequestDelay);

    // 4 requests: "/", "/favicon.ico", "/wait", "/success"
    whenPageLoaded(4);

    ASSERT_EQ(1, pathRequestCount("/success"));
}

} // namespace test

} // namespace nx::vms::client::desktop
