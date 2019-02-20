
#include "functional_tests/mediator_functional_test.h"

#include <nx/network/test_support/maintenance_service_acceptance.h>
#include <nx/network/cloud/mediator/api/mediator_api_http_paths.h>

namespace nx::hpm::test {

struct MaintenanceTypeSetImpl
{
    static const char* apiPrefix;
    using BaseType = MediatorFunctionalTest;
};

const char* MaintenanceTypeSetImpl::apiPrefix = nx::hpm::api::kMediatorApiPrefix;

using namespace nx::network::test;

INSTANTIATE_TYPED_TEST_CASE_P(
    ConnectionMediator,
    MaintenanceServiceAcceptance,
    MaintenanceTypeSetImpl);

} // namespace nx::hpm::test