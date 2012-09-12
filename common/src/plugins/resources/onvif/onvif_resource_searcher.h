#ifndef onvif_resource_searcher_h
#define onvif_resource_searcher_h


#include "core/resource/network_resource.h"
#include "core/resourcemanagment/resource_searcher.h"
#include "onvif_resource_searcher_wsdd.h"
//#include "onvif_resource_searcher_mdns.h"


class OnvifResourceSearcher : public QnAbstractNetworkResourceSearcher
{
    OnvifResourceSearcherWsdd& wsddSearcher;
    //OnvifResourceSearcherMdns& mdnsSearcher;

protected:
    OnvifResourceSearcher();

    virtual ~OnvifResourceSearcher();

    virtual void pleaseStop() override;
public:

    static OnvifResourceSearcher& instance();

    bool isProxy() const;

    virtual QnResourceList findResources();

    virtual QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters);

    virtual QString manufacture() const;

    virtual QnResourcePtr checkHostAddr(QHostAddress addr, QAuthenticator auth);
};

#endif // onvif_resource_searcher_h
