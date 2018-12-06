#include "sse_helper.h"

#include <QtCore/private/qsimd_p.h>

#if defined(Q_CC_MSVC)

    #include <intrin.h> //< For __cpuid.

#elif defined(Q_CC_GNU)

    #if defined(__amd64__)
        #if defined(__clang__)
            #define __cpuid(res, op) \
                __asm__ volatile( \
                "cpuid               \n\t" \
                :"=a"(res[0]), "=b"(res[1]), "=c"(res[2]), "=d"(res[3]) \
                :"0"(op) : "%rsi")
        #else
            #define __cpuid(res, op) \
                __asm__ volatile( \
                "mov %%rbx, %%rsi    \n\t" \
                "cpuid               \n\t" \
                "mov %%rbx, %1       \n\t" \
                "mov %%rsi, %%rbx    \n\t" \
                :"=a"(res[0]), "=m"(res[1]), "=c"(res[2]), "=d"(res[3]) \
                :"0"(op) : "%rsi")
        #endif
    #elif defined(__i386) || defined(__amd64) //< TODO: #mshevchenko: Check on "__amd64".
        #define __cpuid(res, op) \
            __asm__ volatile( \
            "movl %%ebx, %%esi   \n\t" \
            "cpuid               \n\t" \
            "movl %%ebx, %1      \n\t" \
            "movl %%esi, %%ebx   \n\t" \
            :"=a"(res[0]), "=m"(res[1]), "=c"(res[2]), "=d"(res[3]) \
            :"0"(op) : "%esi")
    #elif defined(__arm__) || defined(__aarch64__)
        #define __cpuid(res, op) // TODO: ARM
    #elif defined(__APPLE__) && defined(TARGET_OS_IPHONE)
        // not required
    #else
        #error __cpuid is not implemented for target CPU.
    #endif

#else

    #error __cpuid is not supported for your compiler.

#endif

QString getCPUString()
{
    #if defined(__i386) || defined(__amd64) || defined(_WIN32)

        char CPUBrandString[0x40];
        int CPUInfo[4] = {-1};

        // Calling __cpuid with 0x80000000 as the InfoType argument
        // gets the number of valid extended IDs.
        __cpuid(CPUInfo, (quint32) 0x80000000);
        unsigned int nExIds = CPUInfo[0];
        memset(CPUBrandString, 0, sizeof(CPUBrandString));

        // Get the information associated with each extended ID.
        for (unsigned int i = 0x80000000; i <= nExIds; ++i)
        {
            __cpuid(CPUInfo, i);

            // Interpret CPU brand string and cache information.
            if (i == 0x80000002)
                memcpy(CPUBrandString, CPUInfo, sizeof(CPUInfo));
            else if (i == 0x80000003)
                memcpy(CPUBrandString + 16, CPUInfo, sizeof(CPUInfo));
            else if (i == 0x80000004)
                memcpy(CPUBrandString + 32, CPUInfo, sizeof(CPUInfo));
        }

        if (nExIds >= 0x80000004)
        {
            //printf_s("\nCPU Brand String: %s\n", CPUBrandString);
            return QString::fromLatin1(CPUBrandString).trimmed();
        }
        return QString();

    #elif defined(__arm__) || defined(__aarch64__)

        // TODO: ARM
        return QString();

    #else

        // TODO
        return QString();

    #endif
}

// TODO: #ak give up the following Q_OS_MAC check
// TODO: ARM: sse analog

#if defined(__arm__) || defined(__aarch64__) || defined(Q_OS_IOS)
    bool useSSE2() { return false; }
    bool useSSE3() { return false; }
    bool useSSSE3() { return false; }
    bool useSSE41() { return false; }
    bool useSSE42() { return false; }
#elif defined(Q_OS_MACX)
    bool useSSE2() { return true; }
    bool useSSE3() { return true; }
    bool useSSSE3() { return true; }
    // TODO: #ak We are compiling mac client with -msse4.1 - why is it forbidden here?
    bool useSSE41() { return false; }
    bool useSSE42() { return false; }
#else
    bool useSSE2() { return qCpuHasFeature(SSE2); }
    bool useSSE3() { return qCpuHasFeature(SSE3); }
    bool useSSSE3() { return qCpuHasFeature(SSSE3); }
    bool useSSE41() { return qCpuHasFeature(SSE4_1); }
    bool useSSE42() { return qCpuHasFeature(SSE4_2); }
#endif // defined(__arm__)
