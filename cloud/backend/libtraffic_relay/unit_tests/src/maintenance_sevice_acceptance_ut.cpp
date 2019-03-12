#include <gtest/gtest.h>

#include <nx/network/test_support/maintenance_service_acceptance.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_http_paths.h>

#include "basic_component_test.h"

namespace nx::cloud::relay::test {

class MaintenanceServiceBaseTypeImpl:
    public testing::Test
{
public:
    MaintenanceServiceBaseTypeImpl():
        m_test(BasicComponentTest::Mode::singleRelay)
    {
    }

    void addArg(const char* arg)
    {
        m_args.emplace_back(arg);
    }

    bool startAndWaitUntilStarted()
    {
        m_test.addRelayInstance(argsAsConstCharPtr(), /*waitUntilStarted*/ true);
        return true;
    }

    nx::network::SocketAddress httpEndpoint() const
    {
        nx::utils::Url url = m_test.relay(0).basicUrl();
        return nx::network::SocketAddress(url.host() + ":" + QString::number(url.port()));
    }

    QString testDataDir() const
    {
        return m_test.testDataDir();
    }

private:
    std::vector<const char *> argsAsConstCharPtr()
    {
        std::vector<const char *> args;
        std::transform(m_args.begin(), m_args.end(),
            std::back_inserter(args), [](const auto& str) { return str.c_str(); });
        return args;
    }

private:
    BasicComponentTest m_test;
    std::vector<std::string> m_args;
};

//-------------------------------------------------------------------------------------------------

struct MaintenanceTypeSetImpl
{
    static const char* apiPrefix;
    using BaseType = MaintenanceServiceBaseTypeImpl;
};

const char* MaintenanceTypeSetImpl::apiPrefix = nx::cloud::relay::api::kApiPrefix;

using namespace nx::network::test;

INSTANTIATE_TYPED_TEST_CASE_P(
    TrafficRelay,
    MaintenanceServiceAcceptance,
    MaintenanceTypeSetImpl);

} // namespace nx::cloud::relay::test

