#include <QtCore/QCoreApplication>

#include <nx/fusion/serialization/lexical.h>
#include <nx/network/http/http_client.h>
#include <nx/network/http/auth_tools.h>

#define USE_GMOCK
#include <nx/network/test_support/run_test.h>

#include "relay_test_setup.h"

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);

    const auto resultCode = nx::network::test::runTest(
        argc, argv,
        nullptr,
        nx::network::InitializationFlags::disableUdt);

    return resultCode;
}
