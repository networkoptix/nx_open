// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>

#include <core/resource/resource_fwd.h>
#include <nx/network/socket_common.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/api/types/motion_types.h>

namespace nx::network {

class NX_VMS_COMMON_API MulticastAddressRegistry: public QObject
{
    Q_OBJECT

public:
    struct AddressUsageInfo
    {
        QWeakPointer<QnVirtualCameraResource> device;
        nx::vms::api::StreamIndex stream;
    };

    using RegisteredAddressHolder = nx::utils::ScopeGuard<std::function<void()>>;
    using RegisteredAddressHolderPtr = std::unique_ptr<RegisteredAddressHolder>;

public:
    RegisteredAddressHolderPtr registerAddress(
        QnVirtualCameraResourcePtr resource,
        nx::vms::api::StreamIndex streamIndex,
        nx::network::SocketAddress address);

    AddressUsageInfo addressUsageInfo(const nx::network::SocketAddress& address) const;

private:
    bool unregisterAddress(const nx::network::SocketAddress& address);

private:
    mutable nx::Mutex m_mutex;
    std::map<nx::network::SocketAddress, AddressUsageInfo> m_registry;
};

} // namespace nx::network
