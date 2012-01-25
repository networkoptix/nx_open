#ifndef dlink_device_server_h_2219
#define dlink_device_server_h_2219

#include "core/resourcemanagment/resourceserver.h"


class QnPlDlinkResourceSearcher : public QnAbstractNetworkResourceSearcher
{
    QnPlDlinkResourceSearcher();

public:
    static QnPlDlinkResourceSearcher& instance();

    QnResourceList findResources(void);

    QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters);
    // return the manufacture of the server
    virtual QString manufacture() const;

    virtual QnResourcePtr checkHostAddr(QHostAddress addr);
};

#endif // dlink_device_server_h_2219
