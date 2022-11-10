// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "random_cryptographic_device.h"

#if defined(__linux__) && !defined(ANDROID)
    #include <linux/random.h>
    #include <syscall.h>
    #include <unistd.h>
#elif defined(_WIN32)
    #include <stdlib.h>
#else
// Fallback to QRandomGenerator
#endif // _WIN32

#include <QtCore/QRandomGenerator>

#include <nx/utils/std/cpp14.h>

namespace nx {
namespace utils {
namespace random {

namespace {

// SYS_getrandom could be defined, but be empty.
#if defined(__linux__) && (SYS_getrandom + 0 != 0) && !defined(ANDROID)

    // getrandom supported starting with kernel 3.17. E.g., ubuntu 14.04 uses kernel 3.16.
    bool generateSystemDependentRandom(CryptographicDevice::result_type* result)
    {
        int bytesGenerated = syscall(SYS_getrandom, result, sizeof(*result), GRND_NONBLOCK);
        return bytesGenerated == sizeof(*result);
    }

#elif defined(_WIN32)

    bool generateSystemDependentRandom(CryptographicDevice::result_type* result)
    {
        return rand_s(result) == 0;
    }

#else

    // Fallback to QRandomGenerator.
    bool generateSystemDependentRandom(CryptographicDevice::result_type* /*result*/)
    {
        return false;
    }

#endif

} // namespace

CryptographicDevice::CryptographicDevice()
{
}

CryptographicDevice::~CryptographicDevice()
{
}

CryptographicDevice::result_type CryptographicDevice::operator()()
{
    result_type number = 0;
    if (generateSystemDependentRandom(&number))
        return number;

    return QRandomGenerator::global()->generate();
}

double CryptographicDevice::entropy() const
{
    return 0.0;
}

CryptographicDevice& CryptographicDevice::instance()
{
    static CryptographicDevice s_istance;
    return s_istance;
}

} // namespace random
} // namespace utils
} // namespace nx
