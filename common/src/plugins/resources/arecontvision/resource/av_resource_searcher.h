#ifndef av_device_server_h_2107
#define av_device_server_h_2107

#include "core/resourcemanagment/resource_searcher.h"

class QnPlArecontResourceSearcher : public QnAbstractNetworkResourceSearcher
{
    QnPlArecontResourceSearcher();

public:
    static QnPlArecontResourceSearcher& instance();

    QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters);
    // return the manufacture of the server
    virtual QString manufacture() const;

    // returns all available devices
    virtual QnResourceList findResources();

    virtual QnResourcePtr checkHostAddr(QHostAddress addr);
};

#endif // av_device_server_h_2107
