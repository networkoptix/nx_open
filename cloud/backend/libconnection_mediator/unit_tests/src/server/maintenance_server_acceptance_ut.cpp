#include <gtest/gtest.h>

#include "functional_tests/mediator_functional_test.h"

#include <nx/network/http/http_client.h>
#include <nx/network/http/http_types.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/maintenance/server.h>
#include <nx/network/maintenance/request_path.h>
#include <nx/network/maintenance/log/request_path.h>
#include <nx/cloud/mediator/mediator_service.h>
#include <nx/network/cloud/mediator/api/mediator_api_http_paths.h>

#include <nx/network/test_support/maintenance_service_acceptance.h>

namespace nx::hpm::test {

struct MediatorMaintenanceTypeSet
{
    static const char* apiPrefix;
    using BaseType = MediatorFunctionalTest;
};

const char* MediatorMaintenanceTypeSet::apiPrefix = nx::hpm::api::kMediatorApiPrefix;

using namespace nx::network::test;

INSTANTIATE_TYPED_TEST_CASE_P(MediatorMaintenance, MaintenanceServiceAcceptance, MediatorMaintenanceTypeSet);

} // namespace nx::hpm::test