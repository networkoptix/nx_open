// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtCore/QCoreApplication>

#include <nx/network/ssl/certificate.h>

#define USE_GMOCK

#include <nx/network/test_support/run_test.h>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv); //< Required for loading TLS plugins in Qt6.

    return nx::network::test::runTest(
        argc, argv,
        [](const nx::ArgumentParser&)
        {
            nx::network::ssl::useRandomCertificate("nx_network_ut");
            return nx::utils::test::DeinitFunctions();
        },
        nx::network::InitializationFlags::none,
        nx::utils::test::TestRunFlag::throwOnFailure);
}
