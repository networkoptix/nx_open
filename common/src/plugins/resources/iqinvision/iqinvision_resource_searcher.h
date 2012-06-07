#ifndef iq_device_server_h_1825
#define iq_device_server_h_1825

#include "core/resourcemanagment/resource_searcher.h"
//#include "../onvif_old/onvif_device_searcher.h"
#include "../onvif/onvif_special_resource.h"

class QnPlIqResourceSearcher : /*public OnvifResourceSearcher,*/ public OnvifSpecialResource
{
    QnPlIqResourceSearcher();

public:
    static QnPlIqResourceSearcher& instance();

    QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters);

    virtual QnNetworkResourcePtr createResource() const;

    // return the manufacture of the server
    virtual QString manufacture() const;

    virtual QnResourcePtr checkHostAddr(QHostAddress addr);

//protected:
//    QnNetworkResourcePtr processPacket(QnResourceList& result, QByteArray& responseData);
};

#endif //iq_device_server_h_1825
