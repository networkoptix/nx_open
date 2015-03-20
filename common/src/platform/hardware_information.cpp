#include "hardware_information.h"

#if defined(Q_OS_WIN)

    #include <windows.h>

    qint64 HardwareInformation::getInstalledMemory()
    {
        MEMORYSTATUSEX stat;
        statex.dwLength = sizeof(stat);

        if (GlobalMemoryStatusEx(&stat))
            return stat.ullAvailPhys;
        return 0;
    }

    qint64 HardwareInformation::getCpuCoreCount()
    {
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        return info.dwNumberOfProcessors;
    }

#elif defined(Q_OS_LINUX)

    #include <sys/sysinfo.h>
    #include <unistd.h>

    qint64 HardwareInformation::getInstalledMemory()
    {
        struct sysinfo info;
        if (sysinfo(&info) == 0)
            return info.totalram;
        return 0;
    }

    qint64 HardwareInformation::getCpuCoreCount()
    {
        return sysconf(_SC_NPROCESSORS_ONLN);
    }

#elif defined(Q_OS_OSX)

    #include <sys/types.h>
    #include <sys/sysctl.h>

    qint64 HardwareInformation::getInstalledMemory()
    {
        int mib[2] = { CTL_HW, HW_MEMSIZE };
        size_t length = sizeof(int64);

        int64_t memory;
        if (sysctl(mib, 2, &memory, &length, NULL, 0) != -1)
            return length;
        return 0;
    }

    qint64 HardwareInformation::getCpuCoreCount()
    {
        int mib[2] = { CTL_HW, HW_NCPU };
        size_t length = sizeof(int64);

        int64_t cpuCount;
        if (sysctl(mib, 2, &memory, &cpuCount, NULL, 0) != -1)
            return cpuCount;
        return 0;
    }

#else

    qint64 HardwareInformation::getInstalledMemory() { return 0; }
    qint64 HardwareInformation::getCpuCount() { return 0; }

#endif
