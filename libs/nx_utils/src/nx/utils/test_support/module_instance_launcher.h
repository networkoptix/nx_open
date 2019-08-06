#pragma once

#include <vector>

#include <boost/algorithm/string.hpp>

#include <QtCore/QDir>
#include <QtCore/QFile>

#include <nx/utils/std/future.h>
#include <nx/utils/std/thread.h>
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
        nx::utils::promise<void> moduleInstantiatedCreatedPromise;
        auto moduleInstantiatedCreatedFuture = moduleInstantiatedCreatedPromise.get_future();

        m_moduleStartedPromise = std::make_unique<nx::utils::promise<bool>>();

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
            if (m_moduleInstance)
                m_moduleInstance->pleaseStop();
        }

        if (m_moduleProcessThread.joinable())
            m_moduleProcessThread.join();
        m_moduleInstance.reset();
    }

    //!restarts process
    bool restart()
    {
        stop();
        return startAndWaitUntilStarted();
    }

    /**
     * Adds short argument without value (e.g., "-f") or
     * a long argument with value (e.g., --output-file=tmp.txt)
     */
    void addArg(const char* arg)
    {
        auto b = std::back_inserter(m_args);
        *b = strdup(arg);

        m_argTypes.push_back(ArgumentType::name);
    }

    /**
     * Adds short argument with value. E.g., addArg("-o", "tmp.txt").
     */
    void addArg(const char* name, const char* value)
    {
        m_args.push_back(strdup(name));
        m_argTypes.push_back(ArgumentType::name);

        m_args.push_back(strdup(value));
        m_argTypes.push_back(ArgumentType::value);
    }

    /**
     * Given name like "out" removes arguments "-out tmp.txt" and "--out=tmp.txt".
     */
    void removeArgByName(const char* name)
    {
        const std::string shortName = std::string("-") + name;
        const std::string longName = std::string("--") + name + "=";

        for (int i = 0; i < argCount(); ++i)
        {
            if (m_argTypes[i] == ArgumentType::value)
                continue;

            if (m_args[i] == shortName)
            {
                removeArgAt(i);
                if (i < argCount() && m_argTypes[i] == ArgumentType::value)
                    removeArgAt(i);
            }
            else if (boost::starts_with(m_args[i], longName))
            {
                removeArgAt(i);
            }
        }
    }

    void removeArgAt(std::size_t pos)
    {
        m_args.erase(m_args.begin() + pos);
        m_argTypes.erase(m_argTypes.begin() + pos);
    }

    int argCount() const
    {
        return static_cast<int>(m_args.size());
    }

    void clearArgs()
    {
        for (auto ptr: m_args)
            free(ptr);
        m_args.clear();

        m_argTypes.clear();
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
    enum class ArgumentType
    {
        /**
         * Can be short argument name (e.g., "-o") or
         * long argument with value (e.g, --output-file=out.txt).
         */
        name,
        /** Value that follow name. E.g., out.txt. */
        value,
    };

    std::vector<char*> m_args;
    std::vector<ArgumentType> m_argTypes;
    std::unique_ptr<ModuleProcessType> m_moduleInstance;
    nx::utils::thread m_moduleProcessThread;
    std::unique_ptr<nx::utils::promise<bool /*result*/>> m_moduleStartedPromise;
    mutable std::mutex m_mutex;
};

} // namespace test
} // namespace utils
} // namespace nx
