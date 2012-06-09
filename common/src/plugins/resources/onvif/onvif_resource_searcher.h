#ifndef onvif_resource_searcher_h
#define onvif_resource_searcher_h


#include "core/resource/network_resource.h"
#include "core/resourcemanagment/resource_searcher.h"
#include "onvif_resource_searcher_wsdd.h"
#include "onvif_resource_searcher_mdns.h"
#include "onvif_special_resource.h"


class OnvifResourceSearcher : public QnAbstractNetworkResourceSearcher
{
    OnvifResourceSearcherWsdd& wsddSearcher;
    OnvifResourceSearcherMdns& mdnsSearcher;
    OnvifSpecialResourceCreatorPtr specialResourceCreator;

protected:
    OnvifResourceSearcher();

    virtual ~OnvifResourceSearcher();

public:

    static OnvifResourceSearcher& instance();

    //Is not synchronized!!!
    void init(const OnvifSpecialResourceCreatorPtr& creator);

    bool isProxy() const;

    virtual QnResourceList findResources();

    virtual QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters);

    virtual QString manufacture() const;

    virtual QnResourcePtr checkHostAddr(QHostAddress addr);
};

#endif // onvif_resource_searcher_h
