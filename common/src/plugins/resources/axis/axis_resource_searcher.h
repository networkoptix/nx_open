#ifndef axis_device_server_h_2219
#define axis_device_server_h_2219

#include "core/resourcemanagment/resource_searcher.h"
//#include "../onvif_old/onvif_device_searcher.h"
#include "../onvif/onvif_special_resource.h"

class QnPlAxisResourceSearcher : /*public OnvifResourceSearcher,*/ public OnvifSpecialResource
{
    QnPlAxisResourceSearcher();

public:
    static QnPlAxisResourceSearcher& instance();

    QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters);

    virtual QnNetworkResourcePtr createResource() const;

    // return the manufacture of the server
    virtual QString manufacture() const;

    virtual QnResourcePtr checkHostAddr(QHostAddress addr);

//protected:
//    QnNetworkResourcePtr processPacket(QnResourceList& result, QByteArray& responseData);
};

#endif // axis_device_server_h_2219
