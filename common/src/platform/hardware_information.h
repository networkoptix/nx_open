#ifndef HARDWARE_INFORMATION_H
#define HARDWARE_INFORMATION_H

#include <QString>

class HardwareInformation
{
public:
    static qint64 getInstalledMemory();
    static qint64 getCpuCoreCount();
};

#endif // HARDWARE_INFORMATION_H
