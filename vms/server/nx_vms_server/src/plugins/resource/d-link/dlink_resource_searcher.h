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

    QnResourceList findResources(void);

    virtual QnResourcePtr createResource(const QnUuid &resourceTypeId, const QnResourceParams& params) override;

    virtual QList<QnResourcePtr> checkHostAddr(const nx::utils::Url& url, const QAuthenticator& auth, bool doMultichannelCheck) override;
protected:
    // return the manufacture of the server
    virtual QString manufacture() const;
};

#endif // ENABLE_DLINK
