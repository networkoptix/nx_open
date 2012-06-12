#ifndef isd_device_server_h_1936
#define isd_device_server_h_1936

#include "core/resourcemanagment/resource_searcher.h"
#include "../onvif_old/onvif_device_searcher.h"

class QnPlISDResourceSearcher : /*public OnvifResourceSearcher,*/ public OnvifSpecialResource
{
    QnPlISDResourceSearcher();

public:
    static QnPlISDResourceSearcher& instance();

    virtual QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters);

    // return the manufacture of the server
    virtual QString manufacture() const;

    virtual QnResourcePtr checkHostAddr(QHostAddress addr);

protected:
    QnNetworkResourcePtr processPacket(QnResourceList& result, QByteArray& responseData);
};

#endif //isd_device_server_h_1936
