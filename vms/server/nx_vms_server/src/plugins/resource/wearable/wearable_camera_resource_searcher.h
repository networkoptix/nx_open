#pragma once

#include "core/resource_management/resource_searcher.h"
#include <nx/vms/server/server_module_aware.h>

class QnMediaServerModule;

class QnWearableCameraResourceSearcher:
    public QnAbstractNetworkResourceSearcher,
    public /*mixin*/ nx::vms::server::ServerModuleAware
{
    using base_type = QnAbstractNetworkResourceSearcher;
public:
    QnWearableCameraResourceSearcher(QnMediaServerModule* serverModule);
    virtual ~QnWearableCameraResourceSearcher() override;

    virtual QString manufacture() const override;

    virtual bool isResourceTypeSupported(QnUuid resourceTypeId) const override;

    virtual QnResourcePtr createResource(const QnUuid &resourceTypeId, const QnResourceParams& params) override;

    virtual QnResourceList findResources() override;
    virtual QList<QnResourcePtr> checkHostAddr(const nx::utils::Url& url, const QAuthenticator& auth, bool isSearchAction) override;
};
