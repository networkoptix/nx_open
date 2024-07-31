// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtCore/QCoreApplication>

#include <nx/cloud/vms_gateway/test_support/vms_gateway_functional_test.h>
#include <nx/network/ssl/certificate.h>
#include <nx/network/test_support/run_test.h>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    return nx::network::test::runTest(
        argc, argv,
        [](const nx::ArgumentParser& /*args*/)
        {
            nx::network::ssl::useRandomCertificate("vms_gateway_ut");
            return nx::utils::test::DeinitFunctions();
        },
        nx::network::InitializationFlags::none,
        nx::utils::test::TestRunFlag::throwOnFailure);
}
