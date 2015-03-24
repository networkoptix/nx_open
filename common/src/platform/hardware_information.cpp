#include "hardware_information.h"

static const QString CPU_X86       = lit("x86");
static const QString CPU_X86_64    = lit("x86_64");
static const QString CPU_IA64      = lit("ia64");
static const QString CPU_ARM       = lit("arm");
static const QString CPU_UNKNOWN   = lit("unknown");

const HardwareInformation& HardwareInformation::instance()
{
    static const HardwareInformation hwInfo;
    return hwInfo;
}

constexpr const QString& gccCpuArchicture()
{
    #if defined(__i386__)
        return CPU_X86;
    #elif defined(__x86_64__)
        return CPU_X86_64;
    #elif defined(__ia64__)
        return CPU_IA64;
    #elif defined(__arm__) || defined(__aarch64__)
        return CPU_ARM;
    #else
        return CPU_UNKNOWN;
    #endif
}

#if defined(Q_OS_WIN)

    #include <windows.h>

    HardwareInformation::HardwareInformation()
    {
        MEMORYSTATUSEX stat;
        statex.dwLength = sizeof(stat);
        if (GlobalMemoryStatusEx(&stat))
            phisicalMemory = stat.ullAvailPhys;
        else
            phisicalMemory = 0;

        SYSTEM_INFO cpu;
        GetSystemInfo(&cpu);
        switch(cpu)
        {
            case PROCESSOR_ARCHITECTURE_INTEL:
                cpuArchitecture = CPU_X86;
                break;
            case PROCESSOR_ARCHITECTURE_AMD64:
                cpuArchitecture = CPU_X86_64;
                break;
            case PROCESSOR_ARCHITECTURE_IA64:
                cpuArchitecture = CPU_IA64;
                break;
            case PROCESSOR_ARCHITECTURE_AMD64:
                cpuArchitecture = CPU_ARM;
                break;
            default:
                cpuArchitecture = CPU_UNKNOWN;
                break;
        }
        cpuCoreCount = cpu.dwNumberOfProcessors;
    }

#elif defined(Q_OS_LINUX)

    #include <sys/sysinfo.h>
    #include <unistd.h>

    HardwareInformation::HardwareInformation()
    {
        struct sysinfo info;
        if (sysinfo(&info) == 0)
            phisicalMemory = info.totalram;
        else
            phisicalMemory = 0;

        cpuArchitecture = gccCpuArchicture();

        if ((cpuCoreCount = sysconf(_SC_NPROCESSORS_ONLN)) == -1)
            cpuCoreCount = 0; // error
    }

#elif defined(Q_OS_OSX)

    #include <sys/types.h>
    #include <sys/sysctl.h>

    HardwareInformation::HardwareInformation()
    {
        int mibMem[2] = { CTL_HW, HW_MEMSIZE };
        size_t length = sizeof(int64);
        if (sysctl(mibMem, 2, &phisicalMemory, &length, NULL, 0) == -1)
            phisicalMemory = 0;

        cpuArchitecture = gccCpuArchicture();

        int mibCpuN[2] = { CTL_HW, HW_NCPU };
        size_t length = sizeof(int64);
        if (sysctl(mibCpuN, 2, &memory, &cpuCoreCount, NULL, 0) == -1)
            cpuCoreCount = 0;
    }

#else

    HardwareInformation::HardwareInformation()
        : phisicalMemory(0)
        , cpuArchitecture(CPU_UNKNOWN)
        , cpuCoreCount(0)
    {
    }

#endif
