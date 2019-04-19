#pragma once

#include <map>

#include <nx/vms/server/resource/resource_fwd.h>
#include <nx/network/socket_common.h>

namespace nx::vms::server::network {

class MulticastAddressRegistry: public QObject
{
    Q_OBJECT

public:
    bool registerAddress(
        resource::CameraPtr resource,
        nx::network::SocketAddress address);
    bool unregisterAddress(nx::network::SocketAddress address);

    resource::CameraPtr addressUser(const nx::network::SocketAddress& address) const;
private:
    mutable QnMutex m_mutex;
    std::map<nx::network::SocketAddress, QWeakPointer<resource::Camera>> m_registry;
};

} // namespace nx::vms::server::network
