#pragma once

#ifdef ENABLE_MDNS

#include <core/resource/network_resource.h>
#include <core/resource_management/resource_searcher.h>

class QHostAddress;
class QnMediaServerModule;

class QnMdnsResourceSearcher: virtual public QnAbstractNetworkResourceSearcher
{
    using base_type = QnAbstractNetworkResourceSearcher;
public:
    QnMdnsResourceSearcher(QnMediaServerModule* serverModule);
    ~QnMdnsResourceSearcher();

    virtual QnResourceList findResources() override;

    /**
     * Compare two ipv4 addresses.
     * @return How many first numbers are equals.
     */
    static int getSimilarity(const QString& remoteAddress, const QString& localAddress);
private:
    /**
     * @param result Just found resources. In case if same camera has been found on multiple
     *     network interfaces.
     * @param Local address, MDNS response had been received on.
     * @param foundHostAddress Source address of received MDNS packet.
     *     NOTE Searcher MUST not duplicate resources, already present in \a result.
     */
    virtual QList<QnNetworkResourcePtr> processPacket(
        const QByteArray& responseData,
        const QHostAddress& discoveryAddress,
        const QHostAddress& foundHostAddress) = 0;
private:
    QnMediaServerModule* m_serverModule = nullptr;
};

#endif // ENABLE_MDNS
