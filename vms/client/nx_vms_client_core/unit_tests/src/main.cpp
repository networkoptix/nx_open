// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtGui/QGuiApplication>

#include <nx/branding.h>
#include <nx/network/socket_global.h>
#include <nx/utils/test_support/run_test.h>

int main(int argc, char** argv)
{
    // Unit tests should be able to run in headless mode.
    if (qgetenv("QT_QPA_PLATFORM").isEmpty())
        qputenv("QT_QPA_PLATFORM", "offscreen");

    QCoreApplication::setOrganizationName(nx::branding::company());
    QCoreApplication::setApplicationName("Unit tests");
    QGuiApplication application(argc, argv);

    nx::network::SocketGlobals::InitGuard sgGuard(
        nx::network::InitializationFlags::disableUdt);

    return nx::utils::test::runTest(argc, argv);
}
