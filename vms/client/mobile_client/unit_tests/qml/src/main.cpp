// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtQuick/QQuickWindow>
#include <QtQuickControls2/QQuickStyle>
#include <QtQuickTest/quicktest.h>

#include <nx/branding.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log_main.h>

#include "qml_test_setup.h"

int main(int argc, char** argv)
{
    // Unit tests should be able to run in headless mode.
    if (qgetenv("QT_QPA_PLATFORM").isEmpty())
        qputenv("QT_QPA_PLATFORM", "offscreen");

    QQuickWindow::setGraphicsApi(QSGRendererInterface::GraphicsApi::Software);

    // The organization name is required by the application context.
    QCoreApplication::setOrganizationName(nx::branding::company());
    QCoreApplication::setApplicationName("Mobile QML unit tests");

    Q_INIT_RESOURCE(mobile_client_library);

    QQuickStyle::setStyle("Basic");

    nx::log::mainLogger()->setDefaultLevel(nx::log::Level::none);

    // Makes NX_ASSERT fail the current test, since failOnWarning() catches warnings.
    nx::setOnAssertHandler([](const QString& message) { qWarning().noquote() << message; });

    nx::vms::client::mobile::test::QmlTestSetup setup;
    return quick_test_main_with_setup(argc, argv, "mobile_client_qml_ut", ":/tests", &setup);
}
