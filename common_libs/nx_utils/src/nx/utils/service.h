#pragma once

#include <QtCore/QString>

#include "abstract_service_settings.h"
#include "move_only_func.h"
#include "std/future.h"
#include "thread/stoppable.h"

namespace nx {
namespace utils {

class NX_UTILS_API Service:
    public QnStoppable
{
public:
    Service(int argc, char **argv, const QString& applicationDisplayName);
    virtual ~Service() = default;

    virtual void pleaseStop() override;

    void setOnStartedEventHandler(
        nx::utils::MoveOnlyFunc<void(bool /*isStarted*/)> handler);

    int exec();

protected:
    virtual std::unique_ptr<AbstractServiceSettings> createSettings() = 0;
    /**
     * NOTE: Implemention MUST call Service::runMainLoop if all custom initialization succeeded.
     */
    virtual int serviceMain(const AbstractServiceSettings& settings) = 0;

    int runMainLoop();
    bool isTerminated() const;

private:
    int m_argc;
    char** m_argv;
    nx::utils::promise<int> m_processTerminationEvent;
    nx::utils::MoveOnlyFunc<void(bool /*isStarted*/)> m_startedEventHandler;
    const QString m_applicationDisplayName;
    std::atomic<bool> m_isTerminated;

    void initializeLog(const AbstractServiceSettings& settings);
    void reportStartupResult(bool result);
};

} // namespace utils
} // namespace nx
