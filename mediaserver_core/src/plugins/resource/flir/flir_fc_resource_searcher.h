#pragma once

#include <queue>
#include <set>
#include <unordered_map>

#include "flir_nexus_common.h"
#include "flir_fc_private.h"

#include <nx/network/system_socket.h>
#include <core/resource_management/resource_searcher.h>

class SocketAddressHasher
{
public:
    std::size_t operator()(const SocketAddress& socketAdrress) const
    {
        return qHash(socketAdrress.toString());
    };
};

namespace nx {
namespace plugins {
namespace flir {

class FcResourceSearcher: public QnAbstractNetworkResourceSearcher
{
    using DeviceInfo = nx::plugins::flir::fc_private::DeviceInfo;

    struct TimestampedDeviceInfo
    {
        DeviceInfo deviceInfo;
        quint64 timestamp;
    };

public:
    FcResourceSearcher();
    virtual ~FcResourceSearcher();

    virtual QList<QnResourcePtr> checkHostAddr(
        const QUrl& url,
        const QAuthenticator& auth,
        bool doMultichannelCheck) override;

    virtual QnResourceList findResources() override;

    virtual bool isSequential() const override;
    virtual QString manufacture() const override;
    virtual QnResourcePtr createResource(
        const QnUuid &resourceTypeId,
        const QnResourceParams &params) override;

private:
    QnResourcePtr makeResource(
        const DeviceInfo& info,
        const QAuthenticator& auth);

    void initListenerUnsafe();
    nx_http::AsyncHttpClientPtr createHttpClient() const;

    void doNextReceiveUnsafe();
    void receiveFromCallback(
        SystemError::ErrorCode errorCode,
        SocketAddress senderAddress,
        std::size_t bytesRead);

    bool hasValidCacheUnsafe(const SocketAddress& address) const;
    bool isDeviceSupported(const DeviceInfo& deviceInfo) const;
    void handleDeviceInfoResponseUnsafe(
        const SocketAddress& senderAddress,
        nx_http::AsyncHttpClientPtr httpClient);

    void cleanUpEndpointInfoUnsafe(const SocketAddress& endpoint);

private:
    QnUuid m_flirFcTypeId;

    std::unique_ptr<nx::network::UDPSocket> m_receiveSocket;
    nx::Buffer m_receiveBuffer;

    std::unordered_map<SocketAddress, nx_http::AsyncHttpClientPtr, SocketAddressHasher> m_httpClients;
    std::unordered_map<SocketAddress, TimestampedDeviceInfo, SocketAddressHasher> m_deviceInfoCache;
    std::set<SocketAddress> m_requestsInProgress;
    bool m_terminated;

    mutable QnMutex m_mutex;
};

} // namespace flir
} // namespace plugins
} // namespace nx

