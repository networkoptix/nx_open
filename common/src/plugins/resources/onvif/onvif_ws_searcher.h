#ifndef onvif_ws_resource_searcher_h
#define onvif_ws_resource_searcher_h

#include "core/resourcemanagment/resource_searcher.h"


class QnPlOnvifWsSearcher : public QnAbstractNetworkResourceSearcher
{
    QnPlOnvifWsSearcher();

public:
    static QnPlOnvifWsSearcher& instance();

    QnResourceList findResources(void);

    QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters);

    virtual QnResourcePtr checkHostAddr(QHostAddress addr);

protected:
    // return the manufacture of the server
    virtual QString manufacture() const;

};

#endif // dlink_device_server_h_2219
