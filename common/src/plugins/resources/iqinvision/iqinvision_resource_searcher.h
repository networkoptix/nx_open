#ifndef iq_device_server_h_1825
#define iq_device_server_h_1825

#include "core/resourcemanagment/resource_searcher.h"
#include "../mdns/mdns_device_searcher.h"


class QnPlIqResourceSearcher : public QnMdnsResourceSearcher
{
    QnPlIqResourceSearcher();

public:
    static QnPlIqResourceSearcher& instance();

    virtual QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters);

    // return the manufacture of the server
    virtual QString manufacture() const;

    virtual QnResourcePtr checkHostAddr(QHostAddress addr);

protected:
    virtual QnNetworkResourcePtr processPacket(QnResourceList& result, QByteArray& responseData, const QHostAddress& discoveryAddress) override;
};

#endif //iq_device_server_h_1825
