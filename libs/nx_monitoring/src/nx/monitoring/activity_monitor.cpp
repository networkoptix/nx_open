// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "activity_monitor.h"

#include <nx/reflect/string_conversion.h>
#include <nx/utils/log/format.h>
#include <nx/utils/std/algorithm.h>

#if defined(Q_OS_WIN)
    #include <nx/monitoring/monitor_win.h>
    using MonitorImplementation = nx::monitoring::WindowsMonitor;
#elif defined(Q_OS_LINUX)
    #include <nx/monitoring/monitor_linux.h>
    using MonitorImplementation =  nx::monitoring::LinuxMonitor;
#elif defined(Q_OS_MACX)
    #include <nx/monitoring/monitor_mac.h>
    using MonitorImplementation = nx::monitoring::MacMonitor;
#endif

namespace nx::monitoring {

ActivityMonitor::NetworkLoad ActivityMonitor::networkInterfaceLoadOrThrow(
    const QString& interfaceName)
{
    auto totalLoad = totalNetworkLoad();
    auto interfaceLoad = nx::utils::find_if(totalLoad,
        [&interfaceName](const auto& load){ return load.interfaceName == interfaceName; });
    if (interfaceLoad == nullptr)
        throw std::invalid_argument("Interface [" + interfaceName.toStdString() + "] not found");

    return *interfaceLoad;
}

ActivityMonitor::NetworkLoad ActivityMonitor::networkInterfaceLoadOrThrow(
    const nx::utils::MacAddress& macAddress)
{
    auto totalLoad = totalNetworkLoad();
    auto interfaceLoad = nx::utils::find_if(totalLoad,
        [&macAddress](const auto& load){ return load.macAddress == macAddress; });
    if (interfaceLoad == nullptr)
    {
        throw std::invalid_argument(
            "Interface with MAC [" + macAddress.toString().toStdString() + "] not found");
    }

    return *interfaceLoad;
}

QList<ActivityMonitor::NetworkLoad> ActivityMonitor::totalNetworkLoad(NetworkInterfaceTypes types)
{
    QList<NetworkLoad> result;
    for(const NetworkLoad &load: totalNetworkLoad())
        if(load.type & types)
            result.push_back(load);
    return result;
}

QList<ActivityMonitor::PartitionSpace> ActivityMonitor::totalPartitionSpaceInfo(
    PartitionTypes types)
{
    QList<PartitionSpace> result;
    for(const PartitionSpace &partition: totalPartitionSpaceInfo())
        if(partition.type & types)
            result.push_back(partition);
    return result;
}

QString toString(const ActivityMonitor::PartitionSpace& value)
{
    return nx::format("Partition(name='%1', path='%2', type=%3, space=%4/%5)").args(
        value.devName, value.path, nx::reflect::toString(value.type),
        value.freeBytes, value.sizeBytes);
}

std::unique_ptr<ActivityMonitor> ActivityMonitor::createForCurrentPlatform()
{
    return std::make_unique<MonitorImplementation>();
}

ActivityMonitor::PartitionType ActivityMonitor::getPartitionTypeByFsType(const QString& fsTypeName)
{
    static const QHash<QString, ActivityMonitor::PartitionType> kFsTypes = {
        { "apfs", LocalDiskPartition },
        { "ffs", LocalDiskPartition },
        { "hfs", LocalDiskPartition },
        { "ufs", LocalDiskPartition },
        { "rootfs", LocalDiskPartition },
        { "ext3", LocalDiskPartition },
        { "ext2", LocalDiskPartition },
        { "ext4", LocalDiskPartition },
        { "zfs", LocalDiskPartition },
        { "exfat", LocalDiskPartition },
        { "vfat", LocalDiskPartition },
        { "ecryptfs", LocalDiskPartition },
        { "fuseblk", LocalDiskPartition }, //< NTFS.
        { "fuse", LocalDiskPartition },
        { "fusectl", LocalDiskPartition },
        { "xfs", LocalDiskPartition },
        { "fuse.osxfs", LocalDiskPartition }, //< Mounted volumes when Docker host is macOS.
        { "smbfs", NetworkPartition },
        { "nfs", NetworkPartition },
        { "nfs4", NetworkPartition },
        { "nfsd", NetworkPartition },
        { "cifs", NetworkPartition },
        { "ramfs", RamDiskPartition },
        { "tmpfs", RamDiskPartition },
    };
    return kFsTypes.value(fsTypeName, UnknownPartition);
}

} // namespace nx::monitoring
