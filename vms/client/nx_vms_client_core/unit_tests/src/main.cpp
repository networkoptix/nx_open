// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtCore/QCoreApplication>

#include <nx/network/socket_global.h>
#include <nx/utils/test_support/run_test.h>

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);

    nx::network::SocketGlobals::InitGuard sgGuard(
        nx::network::InitializationFlags::disableUdt);

    return nx::utils::test::runTest(argc, argv);
}
