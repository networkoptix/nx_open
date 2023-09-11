// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QByteArray>
#include <QtNetwork/QHostAddress>

#include <nx/network/socket_common.h>

#include "upnp_device_description.h"

namespace nx::network::upnp {

class DeviceSearcher;

/**
 * Receives discovered devices info.
 */
class NX_NETWORK_API SearchHandler
{
public:
    virtual ~SearchHandler() = default;

    /**
     * @param localInterfaceAddress Local interface address, device has been discovered on
     * @param discoveredDevAddress Discovered device address
     * @param devInfo Parameters, received by parsing xmlDevInfo
     * @param xmlDevInfo xml data as defined in [UPnP Device Architecture 1.1, section 2.3]
     * @return true, if device has been recognized and processed successfully, false otherwise.
     *   If true, packet WILL NOT be passed to other processors.
     */
    virtual bool processPacket(
        const QHostAddress& localInterfaceAddress,
        const SocketAddress& discoveredDevAddress,
        const DeviceInfo& devInfo,
        const QByteArray& xmlDevInfo) = 0;
    virtual bool isEnabled() const = 0;
};

class NX_NETWORK_API SearchAutoHandler:
    public SearchHandler
{
public:
    SearchAutoHandler(
        nx::network::upnp::DeviceSearcher* deviceSearcher, const QString& devType = QString());
    virtual ~SearchAutoHandler() override;

    nx::network::upnp::DeviceSearcher* deviceSearcher() const;
private:
    nx::network::upnp::DeviceSearcher* m_deviceSearcher = nullptr;
};

} // namespace nx::network::upnp
