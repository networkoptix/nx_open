#ifndef onvif_device_server_h_2054
#define onvif_device_server_h_2054

#ifdef ENABLE_MDNS

#include <core/resource/network_resource.h>
#include <core/resource_management/resource_searcher.h>

class QnMdnsResourceSearcher : virtual public QnAbstractNetworkResourceSearcher
{
public:
    QnMdnsResourceSearcher();
    ~QnMdnsResourceSearcher();

    bool isProxy() const;

    virtual QnResourceList findResources() override;

protected:
    /*!
        \param result Just found resources. In case if same camera has been found on multiple network interfaces
        \param Local address, MDNS response had been recevied on
        \param foundHostAddress Source address of received MDNS packet
        \note Searcher MUST not duplicate resoures, already present in \a result
    */
    virtual QList<QnNetworkResourcePtr> processPacket(
        QnResourceList& result,
        const QByteArray& responseData,
        const QHostAddress& discoveryAddress,
        const QHostAddress& foundHostAddress ) = 0;
};

bool isNewDiscoveryAddressBetter(const QString& host, const QString& newAddress, const QString& oldAddress);

#endif // ENABLE_MDNS

#endif // avigilon_device_server_h_1809
