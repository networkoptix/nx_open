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

    ModuleLauncher(ModuleLauncher&&) = default;
    ModuleLauncher& operator=(ModuleLauncher&&) = default;

    void start()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            NX_CRITICAL(m_status == Status::idle);
            m_status = Status::starting;
            m_condition.notify_all();
        }

        nx::utils::promise<void> moduleInstantiatedCreatedPromise;
        auto moduleInstantiatedCreatedFuture = moduleInstantiatedCreatedPromise.get_future();
        m_moduleProcessThread = nx::utils::thread(
            [this, &moduleInstantiatedCreatedPromise]()->int
            {
                beforeModuleCreation();

                m_moduleInstance = std::make_unique<ModuleProcessType>(
                    static_cast<int>(m_args.size()), m_args.data());

                beforeModuleStart();

                m_moduleInstance->setOnStartedEventHandler(
                    [this](bool isStarted)
                    {
                        std::unique_lock<std::mutex> lock(m_mutex);
                        NX_CRITICAL(m_status == Status::starting);
                        m_status = isStarted ? Status::running : Status::failed;
                        m_condition.notify_all();
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

        std::unique_lock<std::mutex> lock(m_mutex);
        m_condition.wait_for(lock, initializedMaxWaitTime,
            [this]() { return m_status != Status::starting; });

        return m_status == Status::running;
    }

    void stop()
    {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            if (m_status == Status::idle)
                return;

            m_condition.wait(lock, [this]() { return m_status != Status::starting; });
        }

        m_moduleInstance->pleaseStop();
        m_moduleProcessThread.join();
        m_moduleInstance.reset();

        std::unique_lock<std::mutex> lock(m_mutex);
        m_status = Status::idle;
        m_condition.notify_all();
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
    virtual void beforeModuleStart() {}
    virtual void afterModuleDestruction() {}

private:
    enum class Status
    {
        idle,
        starting,
        running,
        failed,
    };

private:
    std::vector<char*> m_args;
    std::unique_ptr<ModuleProcessType> m_moduleInstance;
    nx::utils::thread m_moduleProcessThread;
    mutable std::mutex m_mutex;
    std::condition_variable m_condition;
    Status m_status = Status::idle;
};

}   // namespace test
}   // namespace utils
}   // namespace nx
