// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>
#include <memory>

#include <gtest/gtest.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QElapsedTimer>
#include <QtCore/QObject>
#include <QtQml/QQmlComponent>

#include <nx/vms/client/desktop/utils/qml_property.h>

#include "qml_test_environment.h"

namespace nx::vms::client::desktop {
namespace test {

TEST(WebEngineViewTest, clipboardIsUnavailableWithoutUserAction)
{
    QmlTestEnvironment env;
    QQmlComponent webViewComponent(
        env.engine(),
        "qrc:/web_engine_view_ut/WebEngineViewClipboardTest.qml",
        QQmlComponent::PreferSynchronous);

    std::unique_ptr<QObject> webView;
    webView.reset(webViewComponent.create());
    ASSERT_TRUE(webView != nullptr) << webViewComponent.errorString().toStdString();

    auto wait =
        [&webView](const QString name)
        {
            static const auto kWaitResultTimeout = std::chrono::milliseconds(1000);

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

} // namespace test
} // namespace nx::vms::client::desktop
