#include "multicast_address_registry.h"

#include <nx/vms/server/resource/camera.h>
#include <nx/utils/log/log.h>

namespace nx::vms::server::network {

MulticastAddressRegistry::RegisteredAddressHolder::RegisteredAddressHolder(
    nx::vms::server::network::MulticastAddressRegistry* addressRegistry,
    nx::network::SocketAddress address)
    :
    m_addressRegistry(addressRegistry),
    m_address(std::move(address))
{
    NX_ASSERT(addressRegistry);
}

MulticastAddressRegistry::RegisteredAddressHolder::~RegisteredAddressHolder()
{
    NX_DEBUG(m_addressRegistry, "Unregistering address %1", m_address);
    if (m_addressRegistry)
        m_addressRegistry->unregisterAddress(m_address);
}

bool MulticastAddressRegistry::RegisteredAddressHolder::operator<(
    const RegisteredAddressHolder& other) const
{
    return m_address < other.m_address;
}

MulticastAddressRegistry::RegisteredAddressHolder::operator bool() const
{
    return m_addressRegistry != nullptr;
}

MulticastAddressRegistry::RegisteredAddressHolderPtr MulticastAddressRegistry::registerAddress(
    resource::CameraPtr resource,
    nx::vms::api::StreamIndex streamIndex,
    nx::network::SocketAddress address)
{
    NX_DEBUG(this, "Registering a multicast address %1, resource %2", address, resource);
    QnMutexLocker lock(&m_mutex);
    const auto it = m_registry.find(address);

    if (it != m_registry.cend())
    {
        const auto& usageInfo = it->second;
        NX_WARNING(this,
            "Multicast address %1 is already registered by resource %2, on stream: %3",
            address, usageInfo.device.toStrongRef(), usageInfo.stream);
        return std::make_unique<RegisteredAddressHolder>();
    }

    NX_DEBUG(this, "Successfully registered a multicast address %1 for resource %2 on stream %3",
        address, resource, streamIndex);
    m_registry[address] = { resource.toWeakRef(), streamIndex };
    return std::make_unique<RegisteredAddressHolder>(this, address);
}

bool MulticastAddressRegistry::unregisterAddress(nx::network::SocketAddress address)
{
    QnMutexLocker lock(&m_mutex);
    NX_DEBUG(this, "Unregistering a multicast address %1", address);
    const auto it = m_registry.find(address);
    if (it == m_registry.cend())
    {
        NX_DEBUG(this, "Multicast address %1 is not registered", address);
        return false;
    }

    NX_DEBUG(this, "Multicast address %1 has been successfully unregistered", address);
    m_registry.erase(address);
    return true;
}

MulticastAddressRegistry::AddressUsageInfo MulticastAddressRegistry::addressUsageInfo(
    const nx::network::SocketAddress& address) const
{
    auto it = m_registry.find(address);
    if (it != m_registry.cend())
        return it->second;

    return it->second;
}

} // namespace nx::vms::server::network
