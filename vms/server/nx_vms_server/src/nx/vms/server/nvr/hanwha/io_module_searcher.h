#pragma once

#include <core/resource_management/resource_searcher.h>
#include <nx/vms/server/server_module_aware.h>

namespace nx::vms::server::nvr::hanwha {

class IoModuleSearcher:
    public QnAbstractResourceSearcher,
    public nx::vms::server::ServerModuleAware
{
public:
    IoModuleSearcher(::QnMediaServerModule* serverModule);

    virtual QString manufacturer() const override;

    virtual QnResourceList findResources() override;

    virtual QnResourcePtr createResource(
        const QnUuid& resourceTypeId, const QnResourceParams& params) override;

    virtual bool isVirtualResource() const override;
};

} // namespace nx::vms::server::nvr::hanwha
