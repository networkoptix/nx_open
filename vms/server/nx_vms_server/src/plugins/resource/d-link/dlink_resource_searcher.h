#pragma once

#ifdef ENABLE_DLINK

#include "core/resource_management/resource_searcher.h"
#include <nx/vms/server/server_module_aware.h>
#include <nx/vms/server/server_module_aware.h>

class QnMediaServerModule;

class QnPlDlinkResourceSearcher:
    public QnAbstractNetworkResourceSearcher,
    public /*mixin*/ nx::vms::server::ServerModuleAware
{

public:
    QnPlDlinkResourceSearcher(QnMediaServerModule* serverModule);

    virtual QnResourcePtr createResource(
        const QnUuid &resourceTypeId, const QnResourceParams& params) override;

    virtual QString manufacture() const override;

    QnResourceList findResources();

    virtual QList<QnResourcePtr> checkHostAddr(
        const nx::utils::Url& url, const QAuthenticator& auth, bool doMultichannelCheck) override;
};

#endif // ENABLE_DLINK
