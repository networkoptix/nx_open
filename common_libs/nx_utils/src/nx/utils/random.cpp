#include "random.h"

#include <algorithm>
#include <mutex>
#include <random>
#include <time.h>

#if defined(Q_OS_MAC)
    #include <pthread.h>
#endif

namespace nx {
namespace utils {
namespace random {

QtDevice::QtDevice()
{
    ::qsrand(::time(NULL));
}

QtDevice::result_type QtDevice::operator()()
{
    return ::qrand();
}

double QtDevice::entropy() const
{
    return 0;
}

std::random_device& device()
{
    // There is a bug in OSX's clang and gcc, so thread_local is not supported :(
    #if !defined(Q_OS_MAC)
        thread_local std::random_device rd;
        return rd;
    #else
        static pthread_key_t key;
        static auto init = pthread_key_create(&key, [](void* p){ if (p) delete p; });
        static_cast<void>(init);

        if (auto rdp = static_cast<std::random_device*>(pthread_getspecific(key)))
            return *rdp;

        const auto rdp = new std::random_device();
        pthread_setspecific(key, rdp);
        return *rdp;
    #endif
}

QtDevice& qtDevice()
{
    // There is a bug in OSX's clang and gcc, so thread_local is not supported :(
    #if !defined(Q_OS_MAC)
        thread_local QtDevice rd;
        return rd;
    #else
        static pthread_key_t key;
        static auto init = pthread_key_create(&key, [](void* p){ if (p) delete p; });
        static_cast<void>(init);

        if (auto rdp = static_cast<std::random_device*>(pthread_getspecific(key)))
            return *rdp;

        const auto rdp = new std::QtDevice();
        pthread_setspecific(key, rdp);
        return *rdp;
    #endif
}

QByteArray generate(std::size_t count, char min, char max)
{
    QByteArray data(static_cast<int>(count), Qt::Uninitialized);
    std::uniform_int_distribution<int> distribution(min, max);
    try
    {
        std::generate(
            data.begin(), data.end(),
            [&distribution] { return distribution(device()); });
    }
    catch (const std::exception&)
    {
        std::generate(
            data.begin(), data.end(),
            [&distribution] { return distribution(qtDevice()); });
    }

    return data;
}

} // namespace random
} // namespace utils
} // namespace nx
