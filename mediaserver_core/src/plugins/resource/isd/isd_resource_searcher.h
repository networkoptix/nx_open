#ifndef isd_device_server_h_1936
#define isd_device_server_h_1936

#ifdef ENABLE_ISD

#include <core/resource_management/resource_searcher.h>
#include <plugins/resource/upnp/upnp_resource_searcher.h>
#include <plugins/resource/mdns/mdns_listener.h>
#include <nx/network/upnp/upnp_search_handler.h>
#include <nx/utils/url.h>

class QnPlISDResourceSearcher:
	public QnAbstractNetworkResourceSearcher,
	public nx_upnp::SearchHandler
{

public:
    QnPlISDResourceSearcher(QnCommonModule* commonModule);

    virtual QnResourcePtr createResource(
        const QnUuid &resourceTypeId,
        const QnResourceParams& params) override;

    // return the manufacture of the server
    virtual QString manufacture() const;

    virtual QnResourceList findResources(void) override;

    virtual QList<QnResourcePtr> checkHostAddr(
        const nx::utils::Url& url,
        const QAuthenticator& auth,
        bool doMultichannelCheck) override;

    //Upnp resource searcher
    virtual bool processPacket(
        const QHostAddress& discoveryAddr,
        const SocketAddress& deviceEndpoint,
        const nx_upnp::DeviceInfo& devInfo,
        const QByteArray& xmlDevInfo) override;

private:

    void createResource(
        const nx_upnp::DeviceInfo& devInfo,
        const QnMacAddress& mac,
        const QAuthenticator& auth,
        QnResourceList& result );

    QList<QnResourcePtr> checkHostAddrInternal(
        const nx::utils::Url& url,
        const QAuthenticator& auth);

    bool testCredentials(
        const nx::utils::Url& url,
        const QAuthenticator& auth);

    void cleanupSpaces(QString& rowWithSpaces) const;

    bool isDwOrIsd(const QString& vendorName, const QString& model) const;

    QnResourcePtr processMdnsResponse(
        const QString& mdnsResponse,
        const QString& mdnsRemoteAddress,
        const QnResourceList& alreadyFoundResources);

private:
	QnResourceList m_foundUpnpResources;
	std::set<QString> m_alreadFoundMacAddresses;
	mutable QnMutex m_mutex;
};

#endif // #ifdef ENABLE_ISD
#endif //isd_device_server_h_1936
