#pragma once

#include <vector>

#include <QtCore/QDir>
#include <QtCore/QFile>

#include "nx/utils/std/future.h"
#include "nx/utils/std/thread.h"
#include <nx/utils/uuid.h>

namespace nx {
namespace utils {
namespace test {

/**
 * Added to help create functional tests when each test wants to start process from scratch.
 */
template<typename ModuleProcessType>
class ModuleLauncher
{
public:
    ModuleLauncher()
    {
    }

    virtual ~ModuleLauncher()
    {
        stop();
        clearArgs();
    }

    void start()
    {
        nx::utils::promise<void> moduleInstantiatedCreatedPromise;
        auto moduleInstantiatedCreatedFuture = moduleInstantiatedCreatedPromise.get_future();

        m_moduleStartedPromise = std::make_unique<nx::utils::promise<bool>>();

        m_moduleProcessThread = nx::utils::thread(
            [this, &moduleInstantiatedCreatedPromise]()->int
            {
                beforeModuleCreation();

                m_moduleInstance = std::make_unique<ModuleProcessType>(
                    static_cast<int>(m_args.size()), m_args.data());
                m_moduleInstance->setOnStartedEventHandler(
                    [this](bool isStarted)
                    {
                        m_moduleStartedPromise->set_value(isStarted);
                    });
                moduleInstantiatedCreatedPromise.set_value();
                auto result = m_moduleInstance->exec();

                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_moduleInstance.reset();
                }
                afterModuleDestruction();
                return result;
            });
        moduleInstantiatedCreatedFuture.wait();
    }

    virtual bool startAndWaitUntilStarted()
    {
        start();
        return waitUntilStarted();
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
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_moduleInstance)
                return;
            m_moduleInstance->pleaseStop();
        }

        m_moduleProcessThread.join();
        m_moduleInstance.reset();
    }

    //!restarts process
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
        m_args.clear();
    }

    std::vector<char*>& args()
    {
        return m_args;
    }

    const std::unique_ptr<ModuleProcessType>& moduleInstance()
    {
        return m_moduleInstance;
    }

    const ModuleProcessType* moduleInstance() const
    {
        return m_moduleInstance.get();
    }

protected:
    virtual void beforeModuleCreation() {}
    virtual void afterModuleDestruction() {}

private:
    std::vector<char*> m_args;
    std::unique_ptr<ModuleProcessType> m_moduleInstance;
    nx::utils::thread m_moduleProcessThread;
    std::unique_ptr<nx::utils::promise<bool /*result*/>> m_moduleStartedPromise;
    mutable std::mutex m_mutex;
};

}   // namespace test
}   // namespace utils
}   // namespace nx
