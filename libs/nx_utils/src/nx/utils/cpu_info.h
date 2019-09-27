#pragma once

#include <thread>

#if !defined(__arm__)
    #if defined(_MSC_VER)
        #include <intrin.h>
    #else
        #include <cpuid.h>
    #endif
#endif

namespace nx::utils {

struct CpuInfo
{
    int logicalCores = 0;
    int physicalCores = 0;
};

CpuInfo cpuInfo()
{
    CpuInfo result;
    result.logicalCores = std::thread::hardware_concurrency();
    result.physicalCores = result.logicalCores;

#if !defined(__arm__)
    int cpuinfo[4];
    #if defined(_MSC_VER)
        __cpuid(cpuinfo, 1);
    #else
        __cpuid(1, cpuinfo[0], cpuinfo[1], cpuinfo[2], cpuinfo[3]);
    #endif
    const bool hasHyperThreading = (cpuinfo[3] & (1 << 28)) > 0;
    if (hasHyperThreading)
        result.physicalCores /= 2;
#endif
    return result;
}

} // namespace nx::utils
