// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef Q_OS_IOS
    #include <QtCore/QProcess>
#endif

#include "hardware_information.h"

#include <thread>

#if !defined(__arm__) && !defined(__aarch64__)
    #if defined(_MSC_VER)
        #include <intrin.h>
    #else
        #include <cpuid.h>
    #endif
#endif

#if defined(Q_OS_WIN)
    #include <windows.h>
#elif defined(Q_OS_LINUX)
    #include <sys/sysinfo.h>
    #include <unistd.h>
    #include <fstream>
#elif defined(Q_OS_MACOS)
    #include <sys/types.h>
    #include <sys/sysctl.h>
#endif

#include <nx/build_info.h>

namespace nx::monitoring {

static const QString CPU_X86 = "x86";
static const QString CPU_X86_64 = "x86_64";
static const QString CPU_IA64 = "ia64";
static const QString CPU_ARM = "arm";
static const QString CPU_ARM6 = "armv6";
static const QString CPU_ARM7 = "armv7";
static const QString CPU_UNKNOWN = "unknown";

const HardwareInformation& HardwareInformation::instance()
{
    static HardwareInformation hwInfo;
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
        #if defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) || defined(__ARM_ARCH_6K__) \
                || defined(__ARM_ARCH_6Z__) || defined(__ARM_ARCH_6ZK__) \
                || (_M_ARM == 6)
            return CPU_ARM6;

        #elif defined(__ARM_ARCH_7__) || defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7R__) \
                || defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7S__) \
                || (_M_ARM == 7)
            return CPU_ARM7;

        #else
            return CPU_ARM;
        #endif
    #else
        return CPU_UNKNOWN;
    #endif
}

#ifndef Q_OS_IOS
    static int runProcessAndReadParamValue(
        const QString& program,
        const QStringList& arguments,
        const QByteArray& paramName,
        char delimiter)
    {
        QProcess process;
        process.start(program, arguments);
        process.waitForFinished();
        const auto outputData = process.readAllStandardOutput();

        for (auto line: outputData.split('\n'))
        {
            line = line.trimmed().toLower();
            if (line.contains(paramName))
            {
                auto values = line.split(delimiter);
                if (values.size() > 1)
                    return values[1].trimmed().toInt();
            }
        }
        return 0;
    }
#endif

static void detectCoreCount(int& logicalCores, int& physicalCores)
{
    logicalCores = std::thread::hardware_concurrency();
    physicalCores = logicalCores;

    if (nx::build_info::isWindows())
    {
        if (auto result = runProcessAndReadParamValue(
            "wmic", {"CPU", "Get", "NumberOfCores", "/Format:List"}, "numberofcores", '='))
        {
            physicalCores = result; //< Result has absolute value.
        }
    }
    else if (nx::build_info::isLinux())
    {
        if (auto result = runProcessAndReadParamValue("lscpu", {}, "per core", ':'))
            physicalCores = logicalCores / result;  //< Result has relative value.
    }
    else if (nx::build_info::isMacOsX())
    {
        if (auto result = runProcessAndReadParamValue(
            "sysctl", {"hw.physicalcpu"}, "hw.physicalcpu", ':'))
        {
            physicalCores = result; //< Result has absolute value.
        }
    }
}

#if defined(Q_OS_WIN)

    static const auto REG_CPU_KEY = L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0\\";
    static const auto REG_CPU_VALUE = L"ProcessorNameString";

    HardwareInformation::HardwareInformation()
    {
        MEMORYSTATUSEX stat;
        stat.dwLength = sizeof(stat);
        physicalMemory = GlobalMemoryStatusEx(&stat) ? stat.ullTotalPhys : 0;
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
        detectCoreCount(logicalCores, physicalCores);
    }

#elif defined(Q_OS_LINUX)

    HardwareInformation::HardwareInformation()
    {
        struct sysinfo sys;
        if (sysinfo(&sys) == 0)
            physicalMemory = (qint64)(sys.totalram) * (qint64)(sys.mem_unit);
        cpuArchitecture = compileCpuArchicture();

        std::ifstream cpu("/proc/cpuinfo");
        std::string line;
        while(std::getline(cpu, line))
        {
            if (line.find("model name") == 0 ||
                line.find("Processor") == 0 /* NX1 (bananapi) */)
            {
                auto model = line.substr(line.find(":") + 2);
                cpuModelName = QString::fromStdString(model);
                break;
            }
        }
        detectCoreCount(logicalCores, physicalCores);
    }

#elif defined(Q_OS_MACOS)

    HardwareInformation::HardwareInformation()
    {
        int mibMem[2] = { CTL_HW, HW_MEMSIZE };
        size_t length = sizeof(physicalMemory);
        if (sysctl(mibMem, 2, &physicalMemory, &length, NULL, 0) == -1)
            physicalMemory = 0;

        cpuArchitecture = compileCpuArchicture();

        char buffer[512];
        size_t size = sizeof(buffer);
        if (sysctlbyname("machdep.cpu.brand_string", &buffer, &size, NULL, 0) != -1)
            cpuModelName = QString::fromLatin1(buffer);
        detectCoreCount(logicalCores, physicalCores);
    }

#else

    HardwareInformation::HardwareInformation()
        : physicalMemory(0)
        , cpuArchitecture(CPU_UNKNOWN)
    {
    }

#endif

} // namespace nx::monitoring
