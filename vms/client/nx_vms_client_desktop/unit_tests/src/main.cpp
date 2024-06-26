// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtQuick/QQuickWindow>
#include <QtWidgets/QApplication>

#include <nx/utils/test_support/run_test.h>

int main(int argc, char** argv)
{
    // Unit tests should be able to run in headless mode.
    if (qgetenv("QT_QPA_PLATFORM").isEmpty())
        qputenv("QT_QPA_PLATFORM", "offscreen");

    QQuickWindow::setGraphicsApi(QSGRendererInterface::GraphicsApi::Software);

    QApplication application(argc, argv);

    return nx::utils::test::runTest(argc, (const char**)argv, nullptr, 0);
}
