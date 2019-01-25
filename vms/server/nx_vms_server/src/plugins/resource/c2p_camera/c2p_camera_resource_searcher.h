#pragma once

#include <core/resource_management/resource_searcher.h>

#include <nx/vms/server/server_module_aware.h>

class QnPlC2pCameraResourceSearcher:
    public QnAbstractNetworkResourceSearcher,
    public /*mixin*/ nx::vms::server::ServerModuleAware
{
public:
    QnPlC2pCameraResourceSearcher(QnMediaServerModule* serverModule);

    virtual QnResourceList findResources(void) override;

    virtual QnResourcePtr createResource(
        const QnUuid &resourceTypeId, const QnResourceParams& params) override;

    virtual QList<QnResourcePtr> checkHostAddr(
        const nx::utils::Url& url, const QAuthenticator& auth, bool doMultichannelCheck) override;

protected:
    // return the manufacture of the server
    virtual QString manufacture() const override;

};
