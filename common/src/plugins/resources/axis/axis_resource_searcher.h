#ifndef axis_device_server_h_2219
#define axis_device_server_h_2219

#include "core/resourcemanagment/resource_searcher.h"
#include "../mdns/mdns_device_searcher.h"


class QnPlAxisResourceSearcher : public QnMdnsResourceSearcher
{
    QnPlAxisResourceSearcher();

public:
    static QnPlAxisResourceSearcher& instance();

    virtual QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters);

    // return the manufacture of the server
    virtual QString manufacture() const;

    virtual QnResourcePtr checkHostAddr(QHostAddress addr, QAuthenticator auth, int port);

protected:
    QList<QnNetworkResourcePtr> processPacket(QnResourceList& result, QByteArray& responseData, const QHostAddress& discoveryAddress);
};

#endif // axis_device_server_h_2219
