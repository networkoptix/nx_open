#include "functional_tests/test_setup.h"

#include <nx/network/test_support/maintenance_service_acceptance.h>
#include <nx/utils/test_support/module_instance_launcher.h>
#include <nx/utils/test_support/test_with_temporary_directory.h>
#include <nx/cloud/db/cloud_db_service.h>
#include <nx/cloud/db/client/cdb_request_path.h>

namespace nx::cloud::db::test {

class MaintenanceServiceBaseType: public CdbFunctionalTest
{
public:
    nx::network::SocketAddress httpEndpoint() const
    {
        return moduleInstance()->impl()->httpEndpoints().front();
    }
};

struct MaintenanceTypeSetImpl
{
    static const char* apiPrefix;
    using BaseType = MaintenanceServiceBaseType;
};

const char* MaintenanceTypeSetImpl::apiPrefix = nx::cloud::db::kApiPrefix;

using namespace nx::network::test;

INSTANTIATE_TYPED_TEST_CASE_P(
    CloudDb,
    MaintenanceServiceAcceptance,
    MaintenanceTypeSetImpl);

} // namespace nx::cloud::db::test