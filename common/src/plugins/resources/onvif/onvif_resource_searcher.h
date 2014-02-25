#ifndef onvif_resource_searcher_h
#define onvif_resource_searcher_h

#ifdef ENABLE_ONVIF

#include "core/resource/network_resource.h"
#include "core/resource_management/resource_searcher.h"
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

    virtual QnResourcePtr createResource(QnId resourceTypeId, const QString& url);

    virtual QString manufacture() const;

    virtual QList<QnResourcePtr> checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck) override;
private:
    QList<QnResourcePtr> checkHostAddrInternal(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck);
};

#endif //ENABLE_ONVIF

#endif // onvif_resource_searcher_h
