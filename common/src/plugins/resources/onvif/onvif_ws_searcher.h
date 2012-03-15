#ifndef onvif_device_server_h_2054
#define onvif_device_server_h_2054


#include "core/resource/network_resource.h"
#include "core/resourcemanagment/resource_searcher.h"


class OnvifResourceSearcher : public QnAbstractNetworkResourceSearcher
{
protected:
    OnvifResourceSearcher();
public:
    

	~OnvifResourceSearcher();

    bool isProxy() const;

    virtual QnResourceList findResources();

protected:
    virtual QnNetworkResourcePtr processPacket(QnResourceList& result, QByteArray& responseData) = 0;
private:
    void checkSocket(QUdpSocket& sock, QnResourceList& result, QHostAddress localAddress);
};

#endif // avigilon_device_server_h_1809
