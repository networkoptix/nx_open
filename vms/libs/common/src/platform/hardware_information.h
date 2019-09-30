#ifndef HARDWARE_INFORMATION_H
#define HARDWARE_INFORMATION_H

#include <QString>

class HardwareInformation
{
public:
    static const HardwareInformation& instance();

    qint64    physicalMemory = 0;
    QString   cpuArchitecture;
    QString   cpuModelName;

    int logicalCores = 0;
    int physicalCores = 0;

private:
    HardwareInformation();
};

#endif // HARDWARE_INFORMATION_H
