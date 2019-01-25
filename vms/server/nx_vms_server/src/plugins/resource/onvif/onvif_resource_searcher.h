#pragma once

#ifdef ENABLE_ONVIF

#include "core/resource/network_resource.h"
#include "core/resource_management/resource_searcher.h"
#include "onvif_resource_searcher_wsdd.h"
#include <nx/vms/server/server_module_aware.h>

namespace nx::vms::server { class Settings; }

class OnvifResourceSearcher:
    public QnAbstractNetworkResourceSearcher,
    public /*mixin*/ nx::vms::server::ServerModuleAware
{
public:
    OnvifResourceSearcher(QnMediaServerModule* serverModule);
    virtual ~OnvifResourceSearcher();

    virtual void pleaseStop() override;

    bool isProxy() const;

    virtual QnResourceList findResources();

    virtual QnResourcePtr createResource(const QnUuid &resourceTypeId, const QnResourceParams& params) override;

    virtual QString manufacture() const override;

    virtual QList<QnResourcePtr> checkHostAddr(const nx::utils::Url& url, const QAuthenticator& auth, bool doMultichannelCheck) override;
protected:
    std::unique_ptr<OnvifResourceInformationFetcher> m_informationFetcher;
private:
    std::unique_ptr<OnvifResourceSearcherWsdd> m_wsddSearcher;
    int autoDetectDevicePort(const nx::utils::Url& url);
    //OnvifResourceSearcherMdns m_mdnsSearcher;

    QList<QnResourcePtr> checkHostAddrInternal( const nx::utils::Url& url, const QAuthenticator& auth, bool doMultichannelCheck );
};

#endif //ENABLE_ONVIF
