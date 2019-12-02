#pragma once

#include <nx/vms/server/server_module_aware.h>

#include <nx/vms/server/nvr/i_service_provider.h>

#include <nx/vms/server/nvr/types.h>

namespace nx::vms::server::nvr { class RootFileSystem; }

namespace nx::vms::server::nvr::hanwha {

class ServiceProvider: public IServiceProvider, ServerModuleAware
{
public:
    ServiceProvider(QnMediaServerModule* serverModule);

    virtual ~ServiceProvider() override;

    virtual std::unique_ptr<IService> createService() const override;
};

} // namespace nx::vms::server::nvr::hanwha
