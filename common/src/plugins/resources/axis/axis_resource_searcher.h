#ifndef axis_device_server_h_2219
#define axis_device_server_h_2219

#include "core/resourcemanagment/resourceserver.h"
#include "../onvif/onvif_device_searcher.h"

class QnPlAxisResourceSearcher : public OnvifResourceSearcher
{
    QnPlAxisResourceSearcher();
public:

    ~QnPlAxisResourceSearcher(){};

    static QnPlAxisResourceSearcher& instance();

    QnResourcePtr createResource(const QnId& resourceTypeId, const QnResourceParameters& parameters);
    // return the manufacture of the server 
    virtual QString manufacture() const;

    virtual QnResourcePtr checkHostAddr(QHostAddress addr);
protected:
    QnNetworkResourcePtr processPacket(QnResourceList& result, QByteArray& responseData);
};

#endif // axis_device_server_h_2219
