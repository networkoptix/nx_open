#include "random.h"

#include <algorithm>
#include <mutex>
#include <random>

#if defined(Q_OS_MAC)
    #include <pthread.h>
#endif

namespace nx {
namespace utils {
namespace random {

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

QByteArray generate(std::size_t count, bool* isOk)
{
    QByteArray data(static_cast<int>(count), Qt::Uninitialized);
    try
    {
        std::uniform_int_distribution<int> distribution(
            std::numeric_limits<std::int8_t>::min(),
            std::numeric_limits<std::int8_t>::max());

        std::generate(
            data.data(), data.data() + count,
            [&distribution] { return distribution(device()); });

        if (isOk)
            *isOk = true;
    }
    catch (const std::exception&)
    {
        if (isOk)
            *isOk = false;
    }

    return data;
}

} // namespace random
} // namespace utils
} // namespace nx
