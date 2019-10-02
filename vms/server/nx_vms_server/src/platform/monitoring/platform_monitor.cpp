#include "platform_monitor.h"

#include <nx/fusion/serialization/json_functions.h>
#include <nx/fusion/serialization/lexical_functions.h>

namespace nx::vms::server {

QList<PlatformMonitor::NetworkLoad> PlatformMonitor::totalNetworkLoad(NetworkInterfaceTypes types) {
    QList<NetworkLoad> result;
    for(const NetworkLoad &load: totalNetworkLoad())
        if(load.type & types)
            result.push_back(load);
    return result;
}

QList<PlatformMonitor::PartitionSpace> PlatformMonitor::totalPartitionSpaceInfo(PartitionTypes types) {
    QList<PartitionSpace> result;
    for(const PartitionSpace &partition: totalPartitionSpaceInfo())
        if(partition.type & types)
            result.push_back(partition);
    return result;
}

QString toString(const PlatformMonitor::PartitionSpace& value)
{
    return lm("Partition(name='%1', path='%2', type=%3, space=%4/%5)").args(
        value.devName, value.path, QnLexical::serialized(value.type),
        value.freeBytes, value.sizeBytes);
}

} // namespace nx::vms::server

#define LEXICAL_VALUES_FOR_PT                               \
    (nx::vms::server::PlatformMonitor::LocalDiskPartition,    "local")     \
    (nx::vms::server::PlatformMonitor::RamDiskPartition,      "ram")       \
    (nx::vms::server::PlatformMonitor::OpticalDiskPartition,  "optical")   \
    (nx::vms::server::PlatformMonitor::SwapPartition,         "swap")      \
    (nx::vms::server::PlatformMonitor::NetworkPartition,      "network")   \
    (nx::vms::server::PlatformMonitor::RemovableDiskPartition,"usb")       \
    (nx::vms::server::PlatformMonitor::UnknownPartition,      "unknown")

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::server::PlatformMonitor, PartitionType, LEXICAL_VALUES_FOR_PT)
QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::server::PlatformMonitor, PartitionTypes, LEXICAL_VALUES_FOR_PT)
