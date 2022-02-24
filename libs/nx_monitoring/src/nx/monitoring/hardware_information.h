// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtCore/QString>

namespace nx::monitoring {

class NX_MONITORING_API HardwareInformation
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

} // namespace nx::monitoring
