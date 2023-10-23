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
        { "smbfs", PartitionType::network},
        { "nfs", PartitionType::network},
        { "nfs4", PartitionType::network},
        { "nfsd", PartitionType::network},
        { "cifs", PartitionType::network},
        { "ramfs", PartitionType::ram},
        { "tmpfs", PartitionType::ram},
    };
    return kFsTypes.value(fsTypeName, PartitionType::unknown);
}

} // namespace nx::monitoring
