// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>
#include <memory>

#include <gtest/gtest.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QElapsedTimer>
#include <QtCore/QObject>
#include <QtQml/QQmlComponent>
#include <QtWebChannel/QWebChannel>

#include <nx/vms/client/desktop/utils/qml_property.h>
#include <nx/vms/client/desktop/webpage/web_engine.h>

#include "qml_test_environment.h"

namespace nx::vms::client::desktop {
namespace test {

namespace {

struct WebViewDeleter
{
    void operator()(QObject* webView) const
    {
        // QWebChannel does not expect signals from a registered QML object while it is being
        // destroyed (an assertion in debug Qt), so deregister the objects first.
        if (const auto webChannel = webView->findChild<QWebChannel*>("nxWebChannel"))
        {
            for (QObject* object: webChannel->registeredObjects())
                webChannel->deregisterObject(object);
        }

        delete webView;
    }
};

using WebViewPtr = std::unique_ptr<QObject, WebViewDeleter>;

} // namespace

class WebEngineViewTest: public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        initWebEngine();
    }
};

TEST_F(WebEngineViewTest, clipboardIsUnavailableWithoutUserAction)
{
    QmlTestEnvironment env;
    QQmlComponent webViewComponent(
        env.engine(),
        "qrc:/web_engine_view_ut/WebEngineViewClipboardTest.qml",
        QQmlComponent::PreferSynchronous);

    WebViewPtr webView;
    webView.reset(webViewComponent.create());
    ASSERT_TRUE(webView != nullptr) << webViewComponent.errorString().toStdString();

    auto wait =
        [&webView](const QString name)
        {
            constexpr std::chrono::milliseconds kWaitResultTimeout = std::chrono::minutes(1);

            QmlProperty<QVariant> result(webView.get(), name);
            result = QVariant();

            QElapsedTimer timer;
            timer.start();
            while (result.value().isNull() && timer.elapsed() < kWaitResultTimeout.count())
                qApp->processEvents();

            return result.value();
        };

    auto invoke =
        [&wait, &webView](const QString& name) -> bool
        {
            QMetaObject::invokeMethod(webView.get(), name.toStdString().c_str());
            const QVariant result = wait("result");
            EXPECT_FALSE(result.isNull());
            return result.toBool();
        };

    ASSERT_TRUE(wait("ready").toBool());

    // Clipboard is only available in user event handlers.
    EXPECT_FALSE(invoke("checkWritingClipboard"));
    EXPECT_FALSE(invoke("checkReadingClipboard"));
    EXPECT_FALSE(invoke("checkWritingTextClipboard"));
    EXPECT_FALSE(invoke("checkReadingTextClipboard"));
    EXPECT_FALSE(invoke("checkPasteCommand"));
}

TEST_F(WebEngineViewTest, apiIsAvailableAfterCrossSiteNavigation)
{
    // The page reports through the web channel when the injected API is initialized.
    static const QByteArray kQml = R"qml(
        import QtQml

        import Nx.Controls

        WebEngineView
        {
            property alias apiInitialized: testObject.apiInitialized

            enableInjections: true

            // See the comment in WebEngineViewClipboardTest.qml.
            QtObject
            {
                id: testObject

                property var apiInitialized: null
            }

            Component.onCompleted: webView.webChannel.registerObjects({"test": testObject})

            function loadPage(baseUrl)
            {
                apiInitialized = null
                webView.loadHtml("<html><script>"
                    + "window.vmsApiInit = () => test.apiInitialized = true"
                    + "</script></html>", baseUrl)
            }
        }
    )qml";

    QmlTestEnvironment env;
    QQmlComponent webViewComponent(env.engine());
    webViewComponent.setData(kQml, QUrl());

    WebViewPtr webView;
    webView.reset(webViewComponent.create());
    ASSERT_TRUE(webView != nullptr) << webViewComponent.errorString().toStdString();

    auto waitForApi = [&webView]()
    {
        constexpr std::chrono::milliseconds kWaitResultTimeout = std::chrono::minutes(1);

        QmlProperty<QVariant> result(webView.get(), "apiInitialized");

        QElapsedTimer timer;
        timer.start();
        while (result.value().isNull() && timer.elapsed() < kWaitResultTimeout.count())
            qApp->processEvents();

        return result.value().toBool();
    };

    // Each base URL is another site, so each load replaces the page frame.
    for (const QString baseUrl: {"https://a.example/", "https://b.example/", "https://a.example/"})
    {
        QMetaObject::invokeMethod(webView.get(), "loadPage", Q_ARG(QVariant, baseUrl));
        EXPECT_TRUE(waitForApi()) << baseUrl.toStdString();
    }
}

} // namespace test
} // namespace nx::vms::client::desktop
