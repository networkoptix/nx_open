#include <nx/utils/test_support/module_instance_launcher.h>

#include <nx/cloud/storage/service/storage_service.h>

namespace nx::cloud::storage::service::test {

class CloudStorageLauncher:
    public nx::utils::test::ModuleLauncher<
        nx::cloud::storage::service::StorageService>
{
};

} // namespace nx::cloud::storage::service::test