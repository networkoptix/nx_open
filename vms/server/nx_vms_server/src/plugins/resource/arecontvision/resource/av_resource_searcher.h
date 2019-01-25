#pragma once

#ifdef ENABLE_ARECONT

#include "core/resource_management/resource_searcher.h"
#include "plugins/resource/arecontvision/resource/av_resource.h"
#include <nx/vms/server/server_module_aware.h>
#include <array>

class QnCommonModule;

class QnPlArecontResourceSearcher:
    public QnAbstractNetworkResourceSearcher,
    public /*mixin*/ nx::vms::server::ServerModuleAware
{
    typedef std::array<unsigned char, 6> MacArray;
    using base_type = QnAbstractNetworkResourceSearcher;
public:
    QnPlArecontResourceSearcher(QnMediaServerModule* serverModule);

    virtual QnResourcePtr createResource(const QnUuid &resourceTypeId,
        const QnResourceParams& params) override;

    // returns all available devices
    virtual QnResourceList findResources();

    virtual QList<QnResourcePtr> checkHostAddr(const nx::utils::Url& url,
        const QAuthenticator& auth, bool doMultichannelCheck) override;
protected:
    // return the manufacture of the server
    virtual QString manufacture() const;

private:
    QnNetworkResourcePtr findResourceHelper(const MacArray& mac,
        const nx::network::SocketAddress& addr);
};

#endif  // ENABLE_ARECONT
