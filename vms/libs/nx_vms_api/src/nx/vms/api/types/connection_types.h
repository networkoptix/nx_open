// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/enum_instrument.h>
#include <nx/utils/serialization/flags.h>

namespace nx::vms::api {

enum class PeerType
{
    /**apidoc
     * %caption PT_NotDefined
     */
    notDefined = -1,

    /**apidoc
     * %caption PT_Server
     */
    server = 0,

    /**apidoc
     * %caption PT_DesktopClient
     */
    desktopClient = 1,

    /**apidoc
     * %caption PT_VideowallClient
     */
    videowallClient = 2,

    /**apidoc
     * %caption PT_OldMobileClient
     */
    oldMobileClient = 3,

    /**apidoc
     * %caption PT_MobileClient
     */
    mobileClient = 4,

    /**apidoc
     * %caption PT_CloudServer
     */
    cloudServer = 5,

    /**apidoc
     * 2.6 or below
     * %caption PT_OldServer
     */
    oldServer = 6
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
    /**apidoc
     * Sync transactions with cloud.
     * %caption MasterCloudSync
     */
    masterCloudSync = 1 << 0,

    /**apidoc
     * %caption NoStorages
     */
    noStorages = 1 << 1,

    /**apidoc
     * %caption noBackupStorages
     */
    noBackupStorages = 1 << 2,

    /**apidoc
     * %caption masterLdapSync
     */
    masterLdapSync = 1 << 3,
};

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(RuntimeFlag*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<RuntimeFlag>;
    return visitor(
        Item{RuntimeFlag::masterCloudSync, "MasterCloudSync"},
        Item{RuntimeFlag::noStorages, "NoStorages"},
        Item{RuntimeFlag::noBackupStorages, "noBackupStorages"},
        Item{RuntimeFlag::masterLdapSync, "masterLdapSync"}
    );
}

Q_DECLARE_FLAGS(RuntimeFlags, RuntimeFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(RuntimeFlags)

} // namespace nx::vms::api
