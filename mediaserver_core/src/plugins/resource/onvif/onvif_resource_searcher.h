#pragma once

#ifdef ENABLE_ONVIF

#include "core/resource/network_resource.h"
#include "core/resource_management/resource_searcher.h"
#include "onvif_resource_searcher_wsdd.h"

namespace nx::mediaserver { class Settings; }

class OnvifResourceSearcher: public QnAbstractNetworkResourceSearcher
{
public:
    OnvifResourceSearcher(QnCommonModule* commonModule, const nx::mediaserver::Settings* settings);
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
private:
    const nx::mediaserver::Settings* m_settings = nullptr;
};

#endif //ENABLE_ONVIF
