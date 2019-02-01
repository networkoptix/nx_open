#include "maintenance_service_acceptance_ut.h"

#include <nx/network/test_support/maintenance_service_acceptance.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_http_paths.h>

namespace nx::cloud::relay::test {

MaintenanceServiceBaseTypeImpl::MaintenanceServiceBaseTypeImpl():
    m_test(BasicComponentTest::Mode::singleRelay)
{
}

void MaintenanceServiceBaseTypeImpl::addArg(const char* arg)
{
    m_args.emplace_back(arg);
}

bool MaintenanceServiceBaseTypeImpl::startAndWaitUntilStarted()
{
    m_test.addRelayInstance(argsAsConstCharPtr(), /*waitUntilStarted*/ true);
    return true;
}

nx::network::SocketAddress MaintenanceServiceBaseTypeImpl::httpEndpoint() const
{
    nx::utils::Url url = m_test.relay(0).basicUrl();
    return nx::network::SocketAddress(url.host() + ":" + QString::number(url.port()));
}

QString MaintenanceServiceBaseTypeImpl::testDataDir() const
{
    return m_test.testDataDir();
}

std::vector<const char *> MaintenanceServiceBaseTypeImpl::argsAsConstCharPtr()
{
    std::vector<const char *> args;
    std::transform(m_args.begin(), m_args.end(),
        std::back_inserter(args), [](const auto& str){ return str.c_str(); });
    return args;
}

//-------------------------------------------------------------------------------------------------

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

