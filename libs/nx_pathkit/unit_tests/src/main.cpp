// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtGui/QGuiApplication>

#include <nx/utils/test_support/run_test.h>

int main(int argc, char** argv)
{
    // Unit tests should be able to run in headless mode.
    if (qgetenv("QT_QPA_PLATFORM").isEmpty())
        qputenv("QT_QPA_PLATFORM", "offscreen");

    QGuiApplication app(argc, argv); //< Required for QRhi offscreen rendering.

    return nx::utils::test::runTest(argc, argv);
}
