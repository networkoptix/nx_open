#pragma once

#if defined(ENABLE_FLIR)

#include <queue>
#include <set>
#include <unordered_map>

#include "flir_nexus_common.h"
#include "flir_fc_private.h"

#include <nx/network/system_socket.h>
#include <nx/network/deprecated/asynchttpclient.h>
#include <core/resource_management/resource_searcher.h>

namespace nx {
namespace plugins {
namespace flir {

class FcResourceSearcher: public QnAbstractNetworkResourceSearcher
{
    using DeviceInfo = nx::plugins::flir::fc_private::DeviceInfo;

    struct TimestampedDeviceInfo
    {
        DeviceInfo deviceInfo;
        quint64 timestamp = 0;
    };

public:
    FcResourceSearcher(QnCommonModule* commonModule);
    virtual ~FcResourceSearcher();

    virtual QList<QnResourcePtr> checkHostAddr(
        const nx::utils::Url& url,
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
    nx::network::http::AsyncHttpClientPtr createHttpClient() const;

    void doNextReceiveUnsafe();
    void receiveFromCallback(
        SystemError::ErrorCode errorCode,
        nx::network::SocketAddress senderAddress,
        std::size_t bytesRead);

    bool hasValidCacheUnsafe(const nx::network::SocketAddress& address) const;
    bool isDeviceSupported(const DeviceInfo& deviceInfo) const;
    void handleDeviceInfoResponseUnsafe(
        const nx::network::SocketAddress& senderAddress,
        nx::network::http::AsyncHttpClientPtr httpClient);

    void cleanUpEndpointInfoUnsafe(const nx::network::SocketAddress& endpoint);

private:
    QnUuid m_flirFcTypeId;

    std::unique_ptr<nx::network::UDPSocket> m_receiveSocket;
    nx::Buffer m_receiveBuffer;

    std::unordered_map<nx::network::SocketAddress, nx::network::http::AsyncHttpClientPtr> m_httpClients;
    std::unordered_map<nx::network::SocketAddress, TimestampedDeviceInfo> m_deviceInfoCache;
    std::set<nx::network::SocketAddress> m_requestsInProgress;
    bool m_terminated;

    mutable QnMutex m_mutex;
};

} // namespace flir
} // namespace plugins
} // namespace nx

#endif // defined(ENABLE_FLIR)
