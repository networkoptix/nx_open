#include "multicast_address_registry.h"

#include <nx/vms/server/resource/camera.h>
#include <nx/utils/log/log.h>

namespace nx::vms::server::network {

bool MulticastAddressRegistry::registerAddress(
    resource::CameraPtr resource,
    nx::network::SocketAddress address)
{
    NX_DEBUG(this, "Registering a multicast address %1, resource %2", address, resource);
    QnMutexLocker lock(&m_mutex);
    const auto it = m_registry.find(address);
    if (it != m_registry.cend())
    {
        NX_WARNING(this, "Multicast address %1 is already registered by resource %2",
            address, it->second);
        return false;
    }

    NX_DEBUG(this, "Successfully registered a multicast address %1 for resource %2",
        address, resource);
    m_registry[address] = resource.toWeakRef();
    return true;
}

bool MulticastAddressRegistry::unregisterAddress(nx::network::SocketAddress address)
{
    QnMutexLocker lock(&m_mutex);
    NX_DEBUG(this, "Unregistering a multicast address %1", address);
    const auto it = m_registry.find(address);
    if (it == m_registry.cend())
    {
        NX_WARNING(this, "Multicast address %1 is not registered", address);
        return false;
    }

    NX_DEBUG(this, "Multicast address %1 has been successfully unregistered", address);
    m_registry.erase(address);
    return true;
}

resource::CameraPtr MulticastAddressRegistry::addressUser(
    const nx::network::SocketAddress& address) const
{
    auto it = m_registry.find(address);
    if (it != m_registry.cend())
        return it->second;

    return it->second.toStrongRef();
}

} // namespace nx::vms::server::network
