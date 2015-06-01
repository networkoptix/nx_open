#include "hardware_information.h"

static const QString CPU_X86       = lit("x86");
static const QString CPU_X86_64    = lit("x86_64");
static const QString CPU_IA64      = lit("ia64");
static const QString CPU_ARM       = lit("arm");
static const QString CPU_ARM6      = lit("armv6");
static const QString CPU_ARM7      = lit("armv7");
static const QString CPU_UNKNOWN   = lit("unknown");

const HardwareInformation& HardwareInformation::instance()
{
    static const HardwareInformation hwInfo;
    return hwInfo;
}

const QString& compileCpuArchicture()
{
    #if defined(__i386__) || defined(_M_IX86)
        return CPU_X86;

    #elif defined(__x86_64__) || defined(_M_AMD64)
        return CPU_X86_64;

    #elif defined(__ia64__) || defined(_M_IX86)
        return CPU_IA64;

    #elif defined(__arm__) || defined(_M_ARM)
        #if defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) || defined(__ARM_ARCH_6K__) || defined(__ARM_ARCH_6Z__) || defined(__ARM_ARCH_6ZK__) || defined(_M_ARM) && (_M_ARM == 6)
            return CPU_ARM6;

        #elif defined(__ARM_ARCH_7__) || defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7S__) || defined(_M_ARM) && (_M_ARM == 7)
            return CPU_ARM7;

        #else
            return CPU_ARM;
        #endif
    #else
        return CPU_UNKNOWN;
    #endif
}

#if defined(Q_OS_WIN)

    #include <windows.h>

    static const auto REG_CPU_KEY = L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0\\";
    static const auto REG_CPU_VALUE = L"ProcessorNameString";

    HardwareInformation::HardwareInformation()
    {
        MEMORYSTATUSEX stat;
        stat.dwLength = sizeof(stat);
		phisicalMemory = GlobalMemoryStatusEx(&stat) ? stat.ullTotalPhys : 0;
        cpuArchitecture = compileCpuArchicture();

        HKEY key;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, REG_CPU_KEY, 0, KEY_READ, &key)
                == ERROR_SUCCESS)
        {
            WCHAR buffer[512];
            DWORD size = sizeof(buffer);
            if (RegQueryValueExW(key, REG_CPU_VALUE, 0, NULL, (LPBYTE)buffer, &size)
                    == ERROR_SUCCESS)
                cpuModelName = QString::fromWCharArray(buffer);
            RegCloseKey(key);
        }
    }

#elif defined(Q_OS_LINUX)

    #include <sys/sysinfo.h>
    #include <unistd.h>
    #include <fstream>

    HardwareInformation::HardwareInformation()
    {
        struct sysinfo sys;
        phisicalMemory = (sysinfo(&sys) == 0) ? sys.totalram : 0;
        cpuArchitecture = compileCpuArchicture();

        std::ifstream cpu("/proc/cpuinfo");
        std::string line;
        while(std::getline(cpu, line))
            if (line.find("model name") == 0)
            {
                auto model = line.substr(line.find(":") + 2);
                cpuModelName = QString::fromStdString(model);
                break;
            }
    }

#elif defined(Q_OS_OSX)

    #include <sys/types.h>
    #include <sys/sysctl.h>

    HardwareInformation::HardwareInformation()
    {
        int mibMem[2] = { CTL_HW, HW_MEMSIZE };
        size_t length = sizeof(phisicalMemory);
        if (sysctl(mibMem, 2, &phisicalMemory, &length, NULL, 0) == -1)
            phisicalMemory = 0;

        cpuArchitecture = compileCpuArchicture();

        char buffer[512];
        size_t size = sizeof(buffer);
        if (sysctlbyname("machdep.cpu.brand_string", &buffer, &size, NULL, 0) != -1)
            cpuModelName = QString::fromLatin1(buffer);
    }

#else

    HardwareInformation::HardwareInformation()
        : phisicalMemory(0)
        , cpuArchitecture(CPU_UNKNOWN)
        , cpuCoreCount(0)
    {
    }

#endif
