#ifndef axis_device_server_h_2219
#define axis_device_server_h_2219

#include "core/resourcemanagment/resource_searcher.h"
#include "../onvif/onvif_device_searcher.h"

class QnPlAxisResourceSearcher : public OnvifResourceSearcher
{
    QnPlAxisResourceSearcher();

public:
    static QnPlAxisResourceSearcher& instance();

    QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters);
    // return the manufacture of the server
    virtual QString manufacture() const;

    virtual QnResourcePtr checkHostAddr(QHostAddress addr);

protected:
    QnNetworkResourcePtr processPacket(QnResourceList& result, QByteArray& responseData, const QHostAddress& sender);
};

#endif // axis_device_server_h_2219
