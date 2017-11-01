#include "cryptographic_random_device.h"

#if defined(__linux__) && !defined(ANDROID)
    #include <linux/random.h>
    #include <syscall.h>
    #include <unistd.h>
#elif defined(_WIN32)
    #define _CRT_RAND_S
    #include <stdlib.h> 
#else
// Fallback to QtDevice
#endif // _WIN32

#include <nx/utils/std/cpp14.h>

#include "qt_random_device.h"

namespace nx {
namespace utils {
namespace random {

namespace {

#if defined(__linux__) && !defined(ANDROID)

    bool generateSystemDependentRandom(CryptographicRandomDevice::result_type* result)
    {
        // getrandom supported starting with kernel 3.17. E.g., ubuntu 14.04 uses kernel 3.16.
        #if defined(SYS_getrandom)
            const int bytesGenerated = syscall(SYS_getrandom, result, sizeof(*result), GRND_NONBLOCK);
            return bytesGenerated == sizeof(*result);
        #else
            return false;
        #endif
    }

#elif defined(_WIN32)

    bool generateSystemDependentRandom(CryptographicRandomDevice::result_type* result)
    {
        return rand_s(result) == 0;
    }

#else

    // Fallback to QtDevice.
    bool generateSystemDependentRandom(CryptographicRandomDevice::result_type* /*result*/)
    {
        return false;
    }

#endif

} // namespace

struct CryptographicRandomDeviceImpl
{
    QtDevice fallback;
};

CryptographicRandomDevice::CryptographicRandomDevice():
    m_impl(std::make_unique<CryptographicRandomDeviceImpl>())
{
}

CryptographicRandomDevice::~CryptographicRandomDevice()
{
}

CryptographicRandomDevice::result_type CryptographicRandomDevice::operator()()
{
    result_type number = 0;
    if (generateSystemDependentRandom(&number))
        return number;

    return m_impl->fallback();
}

double CryptographicRandomDevice::entropy() const
{
    return 0.0;
}

CryptographicRandomDevice& CryptographicRandomDevice::instance()
{
    static CryptographicRandomDevice s_istance;
    return s_istance;
}

} // namespace random
} // namespace utils
} // namespace nx
