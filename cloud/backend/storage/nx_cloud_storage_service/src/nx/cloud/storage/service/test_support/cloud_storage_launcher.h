#include <nx/utils/test_support/module_instance_launcher.h>
#include <nx/cloud/storage/service/storage_service.h>

#include <nx/utils/url.h>

namespace nx::cloud::storage::service::test {

class CloudStorageLauncher:
    public nx::utils::test::ModuleLauncher<service::StorageService>
{
public:
    CloudStorageLauncher();
    ~CloudStorageLauncher();

    nx::utils::Url httpUrl() const;
};

} // namespace nx::cloud::storage::service::test