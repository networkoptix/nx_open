#ifndef dlink_device_server_h_2219
#define dlink_device_server_h_2219

#include "core/resourcemanagment/resourceserver.h"


class QnPlDlinkResourceSearcher : public QnAbstractNetworkResourceSearcher
{
    QnPlDlinkResourceSearcher();
public:

    ~QnPlDlinkResourceSearcher(){};

    static QnPlDlinkResourceSearcher& instance();

    QnResourceList findResources(void);

    QnResourcePtr createResource(const QnId& resourceTypeId, const QnResourceParameters& parameters);
    // return the manufacture of the server 
    virtual QString manufacture() const;

    virtual QnResourcePtr checkHostAddr(QHostAddress addr);
protected:

};

#endif // dlink_device_server_h_2219
