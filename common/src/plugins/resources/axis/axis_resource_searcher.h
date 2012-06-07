#ifndef axis_device_server_h_2219
#define axis_device_server_h_2219

#include "core/resourcemanagment/resource_searcher.h"

class QnPlAxisResourceSearcher : public QnAbstractNetworkResourceSearcher
{
    QnPlAxisResourceSearcher();

public:
    static QnPlAxisResourceSearcher& instance();

    QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters);
    // return the manufacture of the server
    virtual QString manufacture() const;

    virtual QnResourcePtr checkHostAddr(QHostAddress addr);

    virtual QnResourceList findResources() { QnResourceList res; return res; }
};

#endif // axis_device_server_h_2219
