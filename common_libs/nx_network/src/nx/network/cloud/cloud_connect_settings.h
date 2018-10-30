#pragma once

#include <optional>
#include <string>

#include <QtCore/QString>

#include <nx/utils/thread/mutex.h>

namespace nx {
namespace network {
namespace cloud {

class NX_NETWORK_API CloudConnectSettings
{
public:
    static constexpr std::chrono::milliseconds kDefaltDelayBeforeSendingConnectToMediatorOverTcp
        = std::chrono::seconds(3);

    std::string forcedMediatorUrl;
    bool isUdpHpEnabled = true;
    bool isCloudProxyEnabled = true;
    bool isDirectTcpConnectEnabled = true;
    bool useHttpConnectToListenOnRelay = false;
    std::chrono::milliseconds delayBeforeSendingConnectToMediatorOverTcp =
        kDefaltDelayBeforeSendingConnectToMediatorOverTcp;

    CloudConnectSettings() = default;
    CloudConnectSettings(const CloudConnectSettings&);
    CloudConnectSettings& operator=(const CloudConnectSettings&);

    // TODO: #ak Change originatingHostAddressReplacement()
    // to a regular field and make this class a structure.

    /**
     * @param hostAddress will be used by mediator instead of request source address.
     * This is needed when peer is placed in the same LAN as mediator (e.g., vms_gateway).
     * In this case mediator receives sees vms_gateway under local IP address, which is useless for remote peer.
     */
    void replaceOriginatingHostAddress(const QString& hostAddress);
    std::optional<QString> originatingHostAddressReplacement() const;

private:
    std::optional<QString> m_originatingHostAddressReplacement;
    mutable QnMutex m_mutex;
};

} // namespace cloud
} // namespace network
} // namespace nx
