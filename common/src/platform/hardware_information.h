#ifndef HARDWARE_INFORMATION_H
#define HARDWARE_INFORMATION_H

#include <QString>

class HardwareInformation
{
public:
    static const HardwareInformation& instance();

    qint64    physicalMemory;
    QString   cpuArchitecture;
    QString   cpuModelName;

private:
    HardwareInformation();
};

#endif // HARDWARE_INFORMATION_H
