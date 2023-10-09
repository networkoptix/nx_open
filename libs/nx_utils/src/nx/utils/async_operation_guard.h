// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/utils/thread/mutex.h>

namespace nx {
namespace utils {

/** Async Operation Guard helps to synchronize async operations
 *  \code
 *      const auto sharedGuard = m_asyncOperationGuard.sharedGuard();
 *      doAsyncOperation([sharedGuard]() {
 *          if (auto lock = sharedGuard->lock()) {
 *              // do stuff with object holding m_asyncOperationGuard safe
 *          }
 *      });
 *  \endcode
 */
class NX_UTILS_API AsyncOperationGuard
{
public:
    AsyncOperationGuard();
    /**
     * Blocks while SharedGuard::Lock instance is alive.
     */
    ~AsyncOperationGuard();

    /**
     * The guard shall be passed in every async operation handler to be able to
     * find out if master object has been deleted.
     */
    class NX_UTILS_API SharedGuard
    {
        friend class AsyncOperationGuard;

        SharedGuard();
        SharedGuard(const SharedGuard&) = delete;
        SharedGuard(SharedGuard&&) = delete;
        SharedGuard& operator=(const SharedGuard&) = delete;
        SharedGuard& operator=(SharedGuard&&) = delete;

    public:
        /**
         * This lock is supposed to be held as long as operation is in progress
         * NOTE: the lock must be verified right after obtaining to find
         * out if operation has been canceled.
         */
        class NX_UTILS_API Lock
        {
            friend class SharedGuard;

            Lock(SharedGuard* sharedGuard);
            Lock(const Lock&) = delete;
            Lock& operator=(const Lock&) = delete;
            Lock& operator=(Lock&&) = delete;

        public:
            Lock(Lock&& rhs) noexcept;
            void unlock();
            operator bool() const;
            bool operator!() const;
            ~Lock();

        private:
            SharedGuard* m_sharedGuard;
            bool m_locked;
        };

        Lock lock();

        /**
         * Causes all further created lock be invalid.
         */
        void terminate();

    private:
        bool begin();
        void end();

        nx::Mutex m_mutex;
        bool m_terminated;
    };

    const std::shared_ptr<SharedGuard>& sharedGuard();

    /**
     * Terminates current async guard and creates a new one.
     */
    void reset();

    /**
     * Forward all of the sharedGuard public methods.
     */
    SharedGuard* operator->() const;

    template <typename Handler>
    auto wrap(Handler&& handler)
    {
        return [sharedGuard = m_sharedGuard, handler = std::forward<Handler>(handler)](
            auto&&... args) mutable
        {
            if (const auto lock = sharedGuard->lock())
                std::move(handler)(std::forward<decltype(args)>(args)...);
        };
    }

private:
    std::shared_ptr<SharedGuard> m_sharedGuard;
};

} // namespace utils
} // namespace nx
