#ifndef isd_device_server_h_1936
#define isd_device_server_h_1936

#ifdef ENABLE_ISD

#include <core/resource_management/resource_searcher.h>
#include <plugins/resource/upnp/upnp_resource_searcher.h>
#include <plugins/resource/mdns/mdns_listener.h>

class QnPlISDResourceSearcher :
    public QnUpnpResourceSearcherAsync
{

public:
    QnPlISDResourceSearcher();

    virtual QnResourcePtr createResource(
        const QnUuid &resourceTypeId,
        const QnResourceParams& params) override;

    // return the manufacture of the server
    virtual QString manufacture() const;

    virtual QnResourceList findResources(void) override;

    virtual QList<QnResourcePtr> checkHostAddr(
        const QUrl& url,
        const QAuthenticator& auth,
        bool doMultichannelCheck) override;

protected:

    //Upnp resource searcher
    virtual void processPacket(
        const QHostAddress& discoveryAddr,
        const SocketAddress& deviceEndpoint,
        const nx_upnp::DeviceInfo& devInfo,
        const QByteArray& xmlDevInfo,
        QnResourceList& result) override;

private:

    void createResource(
        const nx_upnp::DeviceInfo& devInfo,
        const QnMacAddress& mac,
        const QAuthenticator& auth,
        QnResourceList& result );

    QList<QnResourcePtr> checkHostAddrInternal(
        const QUrl& url,
        const QAuthenticator& auth);

    bool testCredentials(
        const QUrl& url,
        const QAuthenticator& auth);

    void cleanupSpaces(QString& rowWithSpaces) const;

    bool isDwOrIsd(const QString& vendorName, const QString& model) const;

    QnResourcePtr processMdnsResponse (
        const QnMdnsListener::ConsumerData& mdnsResponse,
        const QnResourceList& alreadyFoundResources);
};

#endif // #ifdef ENABLE_ISD
#endif //isd_device_server_h_1936
