// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/enum_instrument.h>
#include <nx/utils/serialization/flags.h>

namespace nx::vms::api {

enum class PeerType
{
    notDefined = -1,
    server = 0,
    desktopClient = 1,
    videowallClient = 2,
    oldMobileClient = 3,
    mobileClient = 4,
    cloudServer = 5,
    oldServer = 6 //< 2.6 or below
};

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(PeerType*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<PeerType>;
    return visitor(
        Item{PeerType::notDefined, "PT_NotDefined"},
        Item{PeerType::server, "PT_Server"},
        Item{PeerType::desktopClient, "PT_DesktopClient"},
        Item{PeerType::videowallClient, "PT_VideowallClient"},
        Item{PeerType::oldMobileClient, "PT_OldMobileClient"},
        Item{PeerType::mobileClient, "PT_MobileClient"},
        Item{PeerType::cloudServer, "PT_CloudServer"},
        Item{PeerType::oldServer, "PT_OldServer"}
    );
}

enum class RuntimeFlag
{
    /** Sync transactions with cloud. */
    masterCloudSync = 1 << 0,
    noStorages = 1 << 1,
    noBackupStorages = 1 << 2,
};

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(RuntimeFlag*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<RuntimeFlag>;
    return visitor(
        Item{RuntimeFlag::masterCloudSync, "MasterCloudSync"},
        Item{RuntimeFlag::noStorages, "NoStorages"},
        Item{RuntimeFlag::noBackupStorages, "noBackupStorages"}
    );
}

Q_DECLARE_FLAGS(RuntimeFlags, RuntimeFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(RuntimeFlags)

} // namespace nx::vms::api
