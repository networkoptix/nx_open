/**********************************************************
* May 19, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <vector>

#include <QtCore/QDir>
#include <QtCore/QFile>

#include "nx/utils/std/future.h"
#include "nx/utils/std/thread.h"


namespace nx {
namespace utils {
namespace test {

/** Added to help create functional tests when each test want to start process from scratch */
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
        for (auto ptr: m_args)
            free(ptr);
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
                    [this](bool result)
                    {
                        m_moduleStartedPromise->set_value(result);
                    });
                moduleInstantiatedCreatedPromise.set_value();
                return m_moduleInstance->exec();
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
        static const std::chrono::seconds initializedMaxWaitTime(15);

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
    }

    //!restarts process
    void restart()
    {
        stop();
        start();
        waitUntilStarted();
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

protected:
    const std::unique_ptr<ModuleProcessType>& moduleInstance()
    {
        return m_moduleInstance;
    }

private:
    std::vector<char*> m_args;
    std::unique_ptr<ModuleProcessType> m_moduleInstance;
    nx::utils::thread m_moduleProcessThread;
    std::unique_ptr<nx::utils::promise<bool /*result*/>> m_moduleStartedPromise;
};

}   // namespace test
}   // namespace utils
}   // namespace nx
