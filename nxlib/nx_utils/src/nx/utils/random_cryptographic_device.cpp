#include "random_cryptographic_device.h"

#if defined(__linux__) && !defined(ANDROID)
    #include <linux/random.h>
    #include <syscall.h>
    #include <unistd.h>
#elif defined(_WIN32)
    #include <stdlib.h>
#else
// Fallback to QtDevice
#endif // _WIN32

#include <nx/utils/std/cpp14.h>

#include "random_qt_device.h"

namespace nx {
namespace utils {
namespace random {

namespace {

#if defined(__linux__) && defined(SYS_getrandom) && !defined(ANDROID)

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

    // Fallback to QtDevice.
    bool generateSystemDependentRandom(CryptographicDevice::result_type* /*result*/)
    {
        return false;
    }

#endif

} // namespace

struct CryptographicDeviceImpl
{
    QtDevice fallback;
};

CryptographicDevice::CryptographicDevice():
    m_impl(std::make_unique<CryptographicDeviceImpl>())
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

    return m_impl->fallback();
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
