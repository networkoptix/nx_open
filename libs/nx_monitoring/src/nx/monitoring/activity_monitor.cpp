// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "activity_monitor.h"

#include <ranges>
#include <stdexcept>
#include <unordered_map>

#include <nx/ranges.h>
#include <nx/reflect/string_conversion.h>
#include <nx/utils/log/format.h>

#if defined(Q_OS_WIN)
    #include <nx/monitoring/monitor_win.h>
    using MonitorImplementation = nx::monitoring::WindowsMonitor;
#elif defined(Q_OS_LINUX)
    #include <nx/monitoring/monitor_linux.h>
    using MonitorImplementation =  nx::monitoring::LinuxMonitor;
#elif defined(Q_OS_MACOS)
    #include <nx/monitoring/monitor_mac.h>
    using MonitorImplementation = nx::monitoring::MacMonitor;
#endif

namespace nx::monitoring {

ActivityMonitor::NetworkLoad ActivityMonitor::networkInterfaceLoadOrThrow(
    std::string_view interfaceName)
{
    const std::vector totalLoad = totalNetworkLoad();
    const auto interfaceLoad = std::ranges::find(totalLoad, interfaceName, &NetworkLoad::interfaceName);
    if (interfaceLoad == totalLoad.end())
        throw std::invalid_argument("Interface [" + std::string(interfaceName) + "] not found");

    return *interfaceLoad;
}

ActivityMonitor::NetworkLoad ActivityMonitor::networkInterfaceLoadOrThrow(
    const nx::utils::MacAddress& macAddress)
{
    const std::vector totalLoad = totalNetworkLoad();
    const auto interfaceLoad = std::ranges::find(totalLoad, macAddress, &NetworkLoad::macAddress);
    if (interfaceLoad == totalLoad.end())
        throw std::invalid_argument(
            "Interface with MAC [" + macAddress.toStdString() + "] not found");

    return *interfaceLoad;
}

std::vector<ActivityMonitor::NetworkLoad> ActivityMonitor::totalNetworkLoad(
    NetworkInterfaceTypes types)
{
    return totalNetworkLoad()
        | std::views::filter(
            [types](const NetworkLoad& load) { return types.testFlag(load.type); })
        | nx::ranges::to<std::vector>();
}

std::vector<ActivityMonitor::PartitionSpace> ActivityMonitor::totalPartitionSpaceInfo(
    PartitionTypes types)
{
    return totalPartitionSpaceInfo()
        | std::views::filter(
            [types](const PartitionSpace& partition) { return types.testFlag(partition.type); })
        | nx::ranges::to<std::vector>();
}

std::string toString(const ActivityMonitor::PartitionSpace& value)
{
    return nx::format("Partition(name='%1', path='%2', type=%3, space=%4/%5)")
        .args(value.devName,
            value.path.string(),
            nx::reflect::toString(value.type),
            value.freeBytes,
            value.sizeBytes)
        .toStdString();
}

std::unique_ptr<ActivityMonitor> ActivityMonitor::createForCurrentPlatform()
{
    return std::make_unique<MonitorImplementation>();
}

ActivityMonitor::PartitionType ActivityMonitor::getPartitionTypeByFsType(
    std::string_view fsTypeName)
{
    static const std::unordered_map<std::string_view, ActivityMonitor::PartitionType> kFsTypes = {
        { "apfs", PartitionType::local},
        { "ffs", PartitionType::local},
        { "hfs", PartitionType::local},
        { "ufs", PartitionType::local},
        { "rootfs", PartitionType::local},
        { "ext3", PartitionType::local},
        { "ext2", PartitionType::local},
        { "ext4", PartitionType::local},
        { "zfs", PartitionType::local},
        { "exfat", PartitionType::local},
        { "vfat", PartitionType::local},
        { "ecryptfs", PartitionType::local},
        { "fuseblk", PartitionType::local}, //< NTFS.
        { "fuse", PartitionType::local},
        { "fusectl", PartitionType::local},
        { "xfs", PartitionType::local},
        { "fuse.osxfs", PartitionType::local}, //< Mounted volumes when Docker host is macOS.
        { "overlay", PartitionType::local},
        { "smbfs", PartitionType::network},
        { "nfs", PartitionType::network},
        { "nfs4", PartitionType::network},
        { "nfsd", PartitionType::network},
        { "cifs", PartitionType::network},
        { "ramfs", PartitionType::ram},
        { "tmpfs", PartitionType::ram},
    };
    const auto type = kFsTypes.find(fsTypeName);
    return type != kFsTypes.end() ? type->second : PartitionType::unknown;
}

} // namespace nx::monitoring
