#ifndef axis_device_server_h_2219
#define axis_device_server_h_2219

#ifdef ENABLE_AXIS

#include <map>

#include "core/resource_management/resource_searcher.h"
#include "../mdns/mdns_resource_searcher.h"

class QnPlAxisResourceSearcher : public QnMdnsResourceSearcher
{
    using base_type = QnMdnsResourceSearcher;
    struct TimeMarkedAddress
    {
        nx::network::SocketAddress address;
        nx::utils::ElapsedTimer elapsedTimer;

        TimeMarkedAddress() = default;
        TimeMarkedAddress(const nx::network::SocketAddress& address): address(address)
        {
            elapsedTimer.restart();
        }
    };

public:
    QnPlAxisResourceSearcher(QnMediaServerModule* serverModule);

    virtual QnResourcePtr createResource(
        const QnUuid &resourceTypeId, const QnResourceParams& params) override;

    virtual QString manufacturer() const override;

    virtual QList<QnResourcePtr> checkHostAddr(
        const nx::utils::Url& url, const QAuthenticator& auth, bool doMultichannelCheck) override;

private:
    virtual QList<QnNetworkResourcePtr> processPacket(
        QnResourceList& result,
        const QByteArray& responseData,
        const QHostAddress& discoveryAddress,
        const QHostAddress& foundHostAddress) override;

private:
    nx::network::SocketAddress obtainFixedHostAddress(
        nx::utils::MacAddress discoveredMac, nx::network::SocketAddress discoveredAddress);

    void setChannelToResource(const QnPlAxisResourcePtr& resource, int value);
    bool testCredentials(const QUrl& url, const QAuthenticator& auth) const;

    QAuthenticator determineResourceCredentials(const QnSecurityCamResourcePtr& resource) const;

    template<typename T>
    void addMultichannelResources(QList<T>& result);
private:
    QnMediaServerModule* m_serverModule = nullptr;

    // Maps MACs to IPs, that are non ipv4 link-local.
    // Empty IP means, that MAC never corresponded to the address, that was not link-local ipv4.
    std::map<nx::utils::MacAddress, TimeMarkedAddress> m_foundNonIpv4LinkLocalAddresses;
};

#endif // #ifdef ENABLE_AXIS
#endif // axis_device_server_h_2219
