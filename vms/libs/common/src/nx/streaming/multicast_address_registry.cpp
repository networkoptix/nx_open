#include "multicast_address_registry.h"

#include <nx/utils/log/log.h>
#include <core/resource/camera_resource.h>

namespace nx {
namespace streaming {

MulticastAddressRegistry::MulticastAddressRegistry(QObject* parent):
    QObject(parent),
    QnCommonModuleAware(parent)
{
}

bool MulticastAddressRegistry::registerAddress(
    QnVirtualCameraResourcePtr resource,
    nx::network::SocketAddress address)
{
    NX_DEBUG(this, lm("Registering a multicast address %1, resource %2").args(address, resource));
    QnMutexLocker lock(&m_mutex);
    const auto it = m_registry.find(address);
    if (it != m_registry.cend())
    {
        NX_WARNING(this, lm("Multicast address %1 is already registered by resource %2").args(
            address, it->second));
        return false;
    }

    NX_DEBUG(this, lm("Successfully registered a multicast address %1 for resource %2").args(
        address, resource));
    m_registry[address] = resource.toWeakRef();
    return true;
}

bool MulticastAddressRegistry::unregisterAddress(nx::network::SocketAddress address)
{
    QnMutexLocker lock(&m_mutex);
    NX_DEBUG(this, lm("Unregistering a multicast address %1").args(address));
    const auto it = m_registry.find(address);
    if (it == m_registry.cend())
    {
        NX_WARNING(this, lm("Multicast address %1 is not registered").args(address));
        return false;
    }

    NX_DEBUG(this, lm("Multicast address %1 has been successfully unregistered").args(address));
    m_registry.erase(address);
    return true;
}

QnVirtualCameraResourcePtr MulticastAddressRegistry::addressUser(
    const nx::network::SocketAddress& address) const
{
    auto it = m_registry.find(address);
    if (it != m_registry.cend())
        return it->second;

    return it->second.toStrongRef();
}

} // namespace streaming
} // namespace nx
