#ifndef droid_device_server_h_1755
#define droid_device_server_h_1755

#include "core/resourcemanagment/resource_searcher.h"


class QnPlDroidResourceSearcher : public QnAbstractNetworkResourceSearcher
{
    QnPlDroidResourceSearcher();

public:
    static QnPlDroidResourceSearcher& instance();

    QnResourceList findResources(void);

    QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters);
    // return the manufacture of the server
    virtual QString manufacture() const;

    virtual QnResourcePtr checkHostAddr(QHostAddress addr);

private:
    QUdpSocket m_recvSocket;

};

#endif // droid_device_server_h_1755
