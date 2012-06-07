#ifndef iq_device_server_h_1825
#define iq_device_server_h_1825

#include "core/resourcemanagment/resource_searcher.h"

class QnPlIqResourceSearcher : public QnAbstractNetworkResourceSearcher
{
    QnPlIqResourceSearcher();

public:
    static QnPlIqResourceSearcher& instance();

    QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters);
    // return the manufacture of the server
    virtual QString manufacture() const;

    virtual QnResourcePtr checkHostAddr(QHostAddress addr);

    virtual QnResourceList findResources() { QnResourceList res; return res; }
};

#endif //iq_device_server_h_1825
