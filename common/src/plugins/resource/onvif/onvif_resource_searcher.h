#ifndef onvif_resource_searcher_h
#define onvif_resource_searcher_h

#ifdef ENABLE_ONVIF

#include "core/resource/network_resource.h"
#include "core/resource_management/resource_searcher.h"
#include "onvif_resource_searcher_wsdd.h"
//#include "onvif_resource_searcher_mdns.h"


class OnvifResourceSearcher : public QnAbstractNetworkResourceSearcher
{
public:
    OnvifResourceSearcher();
    virtual ~OnvifResourceSearcher();

    virtual void pleaseStop() override;

    bool isProxy() const;

    virtual QnResourceList findResources();

    virtual QnResourcePtr createResource(const QnUuid &resourceTypeId, const QnResourceParams& params) override;

    virtual QString manufacture() const override;

    virtual QList<QnResourcePtr> checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck) override;
private:
    OnvifResourceSearcherWsdd m_wsddSearcher;
    //OnvifResourceSearcherMdns m_mdnsSearcher;

    QList<QnResourcePtr> checkHostAddrInternal( const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck );
};

#endif //ENABLE_ONVIF

#endif // onvif_resource_searcher_h
