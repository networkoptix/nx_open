#pragma once

#include <map>

#include <nx/vms/server/resource/resource_fwd.h>
#include <nx/network/socket_common.h>

namespace nx::vms::server::network {

class MulticastAddressRegistry: public QObject
{
    Q_OBJECT

public:
    struct AddressUsageInfo
    {
        QWeakPointer<resource::Camera> device;
        nx::vms::api::StreamIndex stream;
    };

    class RegisteredAddressHolder
    {
    public:
        RegisteredAddressHolder() = default;

        RegisteredAddressHolder(
            nx::vms::server::network::MulticastAddressRegistry* addressRegistry,
            nx::network::SocketAddress address);

        ~RegisteredAddressHolder();

        bool operator<(const RegisteredAddressHolder& other) const;

        operator bool() const;

    private:
        nx::vms::server::network::MulticastAddressRegistry* m_addressRegistry = nullptr;
        nx::network::SocketAddress m_address;
    };

    using RegisteredAddressHolderPtr = std::unique_ptr<RegisteredAddressHolder>;

public:
    RegisteredAddressHolderPtr registerAddress(
        resource::CameraPtr resource,
        nx::vms::api::StreamIndex streamIndex,
        nx::network::SocketAddress address);

    AddressUsageInfo addressUsageInfo(const nx::network::SocketAddress& address) const;

private:
    bool unregisterAddress(nx::network::SocketAddress address);

private:
    mutable QnMutex m_mutex;
    std::map<nx::network::SocketAddress, AddressUsageInfo> m_registry;
};

} // namespace nx::vms::server::network
