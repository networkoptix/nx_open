#pragma once

#include <map>

#include <nx/utils/thread/mutex.h>
#include <nx/vms/api/types_fwd.h>
#include <nx/vms/server/resource/resource_fwd.h>
#include <nx/network/socket_common.h>
#include <nx/utils/scope_guard.h>

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

    using RegisteredAddressHolder = nx::utils::ScopeGuard<std::function<void()>>;
    using RegisteredAddressHolderPtr = std::unique_ptr<RegisteredAddressHolder>;

public:
    RegisteredAddressHolderPtr registerAddress(
        resource::CameraPtr resource,
        nx::vms::api::StreamIndex streamIndex,
        nx::network::SocketAddress address);

    AddressUsageInfo addressUsageInfo(const nx::network::SocketAddress& address) const;

private:
    bool unregisterAddress(const nx::network::SocketAddress& address);

private:
    mutable QnMutex m_mutex;
    std::map<nx::network::SocketAddress, AddressUsageInfo> m_registry;
};

} // namespace nx::vms::server::network
