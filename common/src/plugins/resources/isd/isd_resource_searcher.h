#ifndef isd_device_server_h_1936
#define isd_device_server_h_1936

#include "core/resourcemanagment/resource_searcher.h"
#include "../mdns/mdns_device_searcher.h"


class QnPlISDResourceSearcher : public QnMdnsResourceSearcher
{
    QnPlISDResourceSearcher();

public:
    static QnPlISDResourceSearcher& instance();

    virtual QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters);

    // return the manufacture of the server
    virtual QString manufacture() const;

    virtual QnResourcePtr checkHostAddr(QHostAddress addr, QAuthenticator auth);

protected:
    virtual QList<QnNetworkResourcePtr> processPacket(QnResourceList& result, QByteArray& responseData, const QHostAddress& discoveryAddress) override;
};

#endif //isd_device_server_h_1936
