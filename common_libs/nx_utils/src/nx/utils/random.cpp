#include "random.h"

#include <algorithm>
#include <chrono>
#include <mutex>
#include <random>

#if defined(Q_OS_MAC)
    #include <pthread.h>
#endif

namespace nx {
namespace utils {
namespace random {

QtDevice::QtDevice()
{
    const auto time = std::chrono::steady_clock::now() - std::chrono::steady_clock::time_point();
    ::qsrand((uint) std::chrono::duration_cast<std::chrono::milliseconds>(time).count());
}

QtDevice::result_type QtDevice::operator()()
{
    return ::qrand();
}

double QtDevice::entropy() const
{
    return 0;
}

QtDevice& qtDevice()
{
    // There is a bug in OSX's clang and gcc, so thread_local is not supported :(
    #if !defined(Q_OS_MAC)
        thread_local QtDevice rd;
        return rd;
    #else
        static pthread_key_t key;
        static auto init = pthread_key_create(
            &key, [](void* p) { if (p) delete static_cast<QtDevice*>(p); });

        static_cast<void>(init);
        if (auto rdp = static_cast<QtDevice*>(pthread_getspecific(key)))
            return *rdp;

        const auto rdp = new QtDevice();
        pthread_setspecific(key, rdp);
        return *rdp;
    #endif
}

QByteArray generate(std::size_t count, char min, char max)
{
    QByteArray data(static_cast<int>(count), Qt::Uninitialized);
    std::uniform_int_distribution<char> distribution(min, max);

    std::generate(
        data.begin(), data.end(),
        [&distribution]() { return distribution(qtDevice()); });

    return data;
}

} // namespace random
} // namespace utils
} // namespace nx
