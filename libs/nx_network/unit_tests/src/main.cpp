#include <nx/network/ssl/ssl_engine.h>

#define USE_GMOCK

#include <nx/network/test_support/run_test.h>

#include <nx/kit/ini_config.h>

namespace nx::network::test {

struct Ini: nx::kit::IniConfig
{
    Ini(): IniConfig("nx_network_ut.ini") { reload(); }

    NX_INI_FLAG(true, throwOnFailure, "Throw on failure.");
};

} // nx::network::test

int main(int argc, char** argv)
{
    const nx::network::test::Ini ini;

    const int gtestRunFlags = ini.throwOnFailure
        ? nx::utils::test::GtestRunFlag::gtestThrowOnFailure
		: 0;

    return nx::network::test::runTest(
        argc, argv,
        [](const nx::utils::ArgumentParser&)
        {
            nx::network::ssl::Engine::useRandomCertificate("nx_network_ut");
            return nx::utils::test::DeinitFunctions();
        },
        nx::network::InitializationFlags::none,
        gtestRunFlags);
}
