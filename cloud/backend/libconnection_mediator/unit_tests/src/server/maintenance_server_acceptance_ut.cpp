
#include <nx/cloud/mediator/test_support/mediator_functional_test.h>
#include <nx/network/test_support/maintenance_service_acceptance.h>
#include <nx/network/cloud/mediator/api/mediator_api_http_paths.h>

namespace nx::hpm::test {

struct MediatorMaintenanceTypeSet
{
    static const char* apiPrefix;
    using BaseType = nx::hpm::MediatorFunctionalTest;
};

const char* MediatorMaintenanceTypeSet::apiPrefix = nx::hpm::api::kMediatorApiPrefix;

using namespace nx::network::test;

INSTANTIATE_TYPED_TEST_CASE_P(
    MediatorMaintenance,
    MaintenanceServiceAcceptance,
    MediatorMaintenanceTypeSet);

} // namespace nx::hpm::test