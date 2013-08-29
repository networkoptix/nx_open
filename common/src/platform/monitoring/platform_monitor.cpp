#include "platform_monitor.h"

#include "sys_dependent_monitor.h"

QnPlatformMonitor *QnPlatformMonitor::newInstance(QObject *parent) {
    return new QnSysDependentMonitor(parent);
}

QList<QnPlatformMonitor::NetworkLoad> QnPlatformMonitor::totalNetworkLoad(NetworkInterfaceTypes types) {
    QList<NetworkLoad> result;
    foreach(const NetworkLoad &load, totalNetworkLoad())
        if(load.type & types)
            result.push_back(load);
    return result;
}

QList<QnPlatformMonitor::PartitionSpace> QnPlatformMonitor::totalPartitionSpaceInfo(PartitionTypes types) {
    QList<PartitionSpace> result;
    foreach(const PartitionSpace &partition, totalPartitionSpaceInfo())
        if(partition.type & types)
            result.push_back(partition);
    return result;
}

