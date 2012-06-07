#ifndef isd_device_server_h_1936
#define isd_device_server_h_1936

#include "core/resourcemanagment/resource_searcher.h"

class QnPlISDResourceSearcher : public QnAbstractNetworkResourceSearcher
{
    QnPlISDResourceSearcher();

public:
    static QnPlISDResourceSearcher& instance();

    QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters);
    // return the manufacture of the server
    virtual QString manufacture() const;

    virtual QnResourcePtr checkHostAddr(QHostAddress addr);

    virtual QnResourceList findResources() { QnResourceList res; return res; }
};

#endif //isd_device_server_h_1936
