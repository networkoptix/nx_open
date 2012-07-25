#ifndef onvif_device_server_h_2054
#define onvif_device_server_h_2054


#include "core/resource/network_resource.h"
#include "core/resourcemanagment/resource_searcher.h"


class QnMdnsResourceSearcher : public QnAbstractNetworkResourceSearcher
{
protected:
    QnMdnsResourceSearcher();
public:
    

	~QnMdnsResourceSearcher();

    bool isProxy() const;

    virtual QnResourceList findResources();

protected:
    virtual QList<QnNetworkResourcePtr> processPacket(QnResourceList& result, QByteArray& responseData, const QHostAddress& discoveryAddress) = 0;
    bool isNewDiscoveryAddressBetter(const QString& host, const QString& newAddress, const QString& oldAddress);
private:
    void checkSocket(QUdpSocket& sock, QnResourceList& result, QHostAddress localAddress);
};

#endif // avigilon_device_server_h_1809
