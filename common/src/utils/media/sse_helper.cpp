#include "sse_helper.h"

#if defined(Q_CC_MSVC)
#   include <intrin.h> /* For __cpuid. */
#elif defined(Q_CC_GNU)
#   ifdef __amd64__
#       ifdef __clang__
#           define __cpuid(res, op)                                             \
                __asm__ volatile(                                               \
                "cpuid               \n\t"                                      \
                :"=a"(res[0]), "=b"(res[1]), "=c"(res[2]), "=d"(res[3])         \
                :"0"(op) : "%rsi")
#       else
#           define __cpuid(res, op)                                             \
                __asm__ volatile(                                               \
                "mov %%rbx, %%rsi    \n\t"                                      \
                "cpuid               \n\t"                                      \
                "mov %%rbx, %1       \n\t"                                      \
                "mov %%rsi, %%rbx    \n\t"                                      \
                :"=a"(res[0]), "=m"(res[1]), "=c"(res[2]), "=d"(res[3])         \
                :"0"(op) : "%rsi")
#       endif
#   elif defined(__i386) || defined(__amd64)
#       define __cpuid(res, op)                                                 \
            __asm__ volatile(                                                   \
            "movl %%ebx, %%esi   \n\t"                                          \
            "cpuid               \n\t"                                          \
            "movl %%ebx, %1      \n\t"                                          \
            "movl %%esi, %%ebx   \n\t"                                          \
            :"=a"(res[0]), "=m"(res[1]), "=c"(res[2]), "=d"(res[3])             \
            :"0"(op) : "%esi")
#   elif __arm__
#       define __cpuid(res, op)       //TODO/ARM
#   elif defined(__APPLE__) && defined(TARGET_OS_IPHONE)
//      not required
#   else
#       error __cpuid is not implemented for target CPU
#   endif
#else
#   error __cpuid is not supported for your compiler.
#endif

QString getCPUString()
{
#if defined(__i386) || defined(__amd64) || defined(_WIN32)

    char CPUBrandString[0x40];
    int CPUInfo[4] = {-1};

    // Calling __cpuid with 0x80000000 as the InfoType argument
    // gets the number of valid extended IDs.
    __cpuid(CPUInfo, (quint32)0x80000000);
    unsigned int nExIds = CPUInfo[0];
    memset(CPUBrandString, 0, sizeof(CPUBrandString));

    // Get the information associated with each extended ID.
    for(unsigned int i=0x80000000; i<=nExIds; ++i)
    {
        __cpuid(CPUInfo, i);

        // Interpret CPU brand string and cache information.
        if  (i == 0x80000002)
            memcpy(CPUBrandString, CPUInfo, sizeof(CPUInfo));
        else if  (i == 0x80000003)
            memcpy(CPUBrandString + 16, CPUInfo, sizeof(CPUInfo));
        else if  (i == 0x80000004)
            memcpy(CPUBrandString + 32, CPUInfo, sizeof(CPUInfo));
    }

    if(nExIds >= 0x80000004)
    {
        //printf_s("\nCPU Brand String: %s\n", CPUBrandString);
        return QString::fromLatin1(CPUBrandString).trimmed();
    }
    return QString();

#elif __arm__

    //TODO/ARM
    return QString();
#else
    //TODO
    return QString();
#endif
}
