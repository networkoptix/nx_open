#ifndef onvif_device_server_h_2054
#define onvif_device_server_h_2054

#include "core/resource/network_resource.h"
#include "core/resource_management/resource_searcher.h"

bool isNewDiscoveryAddressBetter(const QString& host, const QString& newAddress, const QString& oldAddress);

class QnMdnsResourceSearcher : virtual public QnAbstractNetworkResourceSearcher
{
protected:
    QnMdnsResourceSearcher();
public:
    

    ~QnMdnsResourceSearcher();

    bool isProxy() const;

    virtual QnResourceList findResources();
protected:
    /*!
        \param result Just found resources. In case if same camera has been found on multiple network interfaces
        \note Searcher MUST not duplicate resoures, already present in \a result
    */
    virtual QList<QnNetworkResourcePtr> processPacket(QnResourceList& result, const QByteArray& responseData, const QHostAddress& discoveryAddress) = 0;
};

#endif // avigilon_device_server_h_1809
