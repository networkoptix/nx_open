#pragma once

#include <vector>

#include <QtCore/QDir>
#include <QtCore/QFile>

#include "nx/utils/atomic_unique_ptr.h"
#include "nx/utils/std/future.h"
#include "nx/utils/std/thread.h"

namespace nx {
namespace utils {
namespace test {

/**
 * Added to help create functional tests when each test want to start process from scratch.
 */
template<typename ModuleProcessType>
class ModuleLauncher
{
public:
    ModuleLauncher()
    {
    }

    ~ModuleLauncher()
    {
        stop();
    }

    void start()
    {
        nx::utils::promise<void> moduleInstantiatedCreatedPromise;
        auto moduleInstantiatedCreatedFuture = moduleInstantiatedCreatedPromise.get_future();

        m_moduleStartedPromise = std::make_unique<nx::utils::promise<bool>>();

        m_moduleProcessThread = nx::utils::thread(
            [this, &moduleInstantiatedCreatedPromise]()->int
            {
                m_moduleInstance = std::make_unique<ModuleProcessType>(
                    static_cast<int>(m_args.size()), m_args.data());
                m_moduleInstance->setOnStartedEventHandler(
                    [this](bool isStarted)
                    {
                        m_moduleStartedPromise->set_value(isStarted);
                    });
                moduleInstantiatedCreatedPromise.set_value();
                return m_moduleInstance->exec();
            });
        moduleInstantiatedCreatedFuture.wait();
    }

    virtual bool startAndWaitUntilStarted()
    {
        for (size_t attempt = 0; attempt < ModuleProcessType::kMaxStartRetryCount; ++attempt)
        {
            start();
            if (waitUntilStarted())
                return true;

            stop();
        }

        return false;
    }

    virtual bool waitUntilStarted()
    {
        // NOTE: Valgrind does not stand small timeouts.
        static const std::chrono::minutes initializedMaxWaitTime(1);

        auto moduleStartedFuture = m_moduleStartedPromise->get_future();
        if (moduleStartedFuture.wait_for(initializedMaxWaitTime) !=
            std::future_status::ready)
        {
            return false;
        }

        return moduleStartedFuture.get();
    }

    void stop()
    {
        if (!m_moduleInstance)
            return;

        m_moduleInstance->pleaseStop();
        m_moduleProcessThread.join();
        m_moduleInstance.reset();
        clearArgs();
    }

    bool restart()
    {
        stop();
        return startAndWaitUntilStarted();
    }

    void addArg(const char* arg)
    {
        auto b = std::back_inserter(m_args);
        *b = strdup(arg);
    }

    void addArg(const char* arg, const char* value)
    {
        addArg(arg);
        addArg(value);
    }

    void clearArgs()
    {
        for (auto ptr: m_args)
            free(ptr);

        m_args.resize(0);
    }

    const AtomicUniquePtr<ModuleProcessType>& moduleInstance()
    {
        return m_moduleInstance;
    }

private:
    std::vector<char*> m_args;
    AtomicUniquePtr<ModuleProcessType> m_moduleInstance;
    nx::utils::thread m_moduleProcessThread;
    std::unique_ptr<nx::utils::promise<bool /*result*/>> m_moduleStartedPromise;
};

}   // namespace test
}   // namespace utils
}   // namespace nx
