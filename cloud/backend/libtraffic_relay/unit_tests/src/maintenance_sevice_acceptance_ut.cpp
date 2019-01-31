#include <nx/network/test_support/maintenance_service_acceptance.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_http_paths.h>

#include "basic_component_test.h"

namespace nx::cloud::relay::test {

namespace {

class MaintenanceServiceBaseTypeImpl
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
        args.reserve(m_args.size());
        for (const auto& arg : m_args)
            args.push_back(arg.c_str());
        return args;
    }

private:
    BasicComponentTest m_test;
    std::vector<std::string> m_args;
};

} // namespace

struct TrafficeRelayMaintenanceTypeSet
{
    static const char* apiPrefix;
    using BaseType = MaintenanceServiceBaseTypeImpl;
};

const char* TrafficeRelayMaintenanceTypeSet::apiPrefix = nx::cloud::relay::api::kApiPrefix;

using namespace nx::network::test;

INSTANTIATE_TYPED_TEST_CASE_P(
    TrafficeRelayMaintenance,
    MaintenanceServiceAcceptance,
    TrafficeRelayMaintenanceTypeSet);

} // namespace nx::cloud::relay::test

