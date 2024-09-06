// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <string>

#include <nx/utils/thread/mutex.h>

namespace nx::network::cloud {

class NX_NETWORK_API CloudConnectSettings
{
public:
    std::string forcedMediatorUrl;
    bool isUdpHpEnabled = true;
    bool isUdpHpOverHttpEnabled = true;
    bool isCloudProxyEnabled = true;
    bool isDirectTcpConnectEnabled = true;
    std::chrono::milliseconds delayBeforeSendingConnectToMediatorOverTcp = std::chrono::seconds(3);

    // The timeout for establishing a connection via cloud/NAT traversal .
    std::chrono::milliseconds cloudConnectTimeout = std::chrono::seconds(20);

    CloudConnectSettings() = default;
    CloudConnectSettings(const CloudConnectSettings&);
    CloudConnectSettings& operator=(const CloudConnectSettings&);

    // TODO: #akolesnikov Change originatingHostAddressReplacement()
    // to a regular field and make this class a structure.

    /**
     * @param hostAddress will be used by mediator instead of request source address.
     * This is needed when peer is placed in the same LAN as mediator (e.g., vms_gateway).
     * In this case mediator receives sees vms_gateway under local IP address, which is useless for remote peer.
     */
    void replaceOriginatingHostAddress(const std::string& hostAddress);
    std::optional<std::string> originatingHostAddressReplacement() const;

private:
    std::optional<std::string> m_originatingHostAddressReplacement;
    mutable nx::Mutex m_mutex;
};

} // namespace nx::network::cloud
