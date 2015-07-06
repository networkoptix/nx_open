#include "platform_monitor.h"

#include <utils/serialization/json_functions.h>
#include <utils/serialization/lexical_functions.h>

QList<QnPlatformMonitor::NetworkLoad> QnPlatformMonitor::totalNetworkLoad(NetworkInterfaceTypes types) {
    QList<NetworkLoad> result;
    for(const NetworkLoad &load: totalNetworkLoad())
        if(load.type & types)
            result.push_back(load);
    return result;
}

QList<QnPlatformMonitor::PartitionSpace> QnPlatformMonitor::totalPartitionSpaceInfo(PartitionTypes types) {
    QList<PartitionSpace> result;
    for(const PartitionSpace &partition: totalPartitionSpaceInfo())
        if(partition.type & types)
            result.push_back(partition);
    return result;
}

#define LEXICAL_VALUES_FOR_PT                               \
    (QnPlatformMonitor::LocalDiskPartition,    "local")     \
    (QnPlatformMonitor::RamDiskPartition,      "ram")       \
    (QnPlatformMonitor::OpticalDiskPartition,  "optical")   \
    (QnPlatformMonitor::SwapPartition,         "swap")      \
    (QnPlatformMonitor::NetworkPartition,      "network")   \
    (QnPlatformMonitor::UnknownPartition,      "unknown")

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(QnPlatformMonitor::PartitionType, LEXICAL_VALUES_FOR_PT)
QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(QnPlatformMonitor::PartitionTypes, LEXICAL_VALUES_FOR_PT)