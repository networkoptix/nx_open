#include "service.h"

#include "log/log.h"
#include "log/log_initializer.h"
#include "scope_guard.h"

namespace nx {
namespace utils {

Service::Service(int argc, char **argv, const QString& applicationDisplayName):
    m_argc(argc),
    m_argv(argv),
    m_applicationDisplayName(applicationDisplayName),
    m_isTerminated(false)
{
}

void Service::pleaseStop()
{
    m_isTerminated = true;
    m_processTerminationEvent.set_value(0);
}

void Service::setOnStartedEventHandler(
    nx::utils::MoveOnlyFunc<void(bool /*isStarted*/)> handler)
{
    m_startedEventHandler = std::move(handler);
}

int Service::exec()
{
    auto triggerOnStartedEventHandlerGuard = makeScopeGuard(
        [this]()
        {
            if (m_startedEventHandler)
                m_startedEventHandler(false);
        });

    try
    {
        auto settings = createSettings();
        settings->load(m_argc, const_cast<const char**>(m_argv));
        if (settings->isShowHelpRequested())
        {
            settings->printCmdLineArgsHelp();
            return 0;
        }

        initializeLog(*settings);

        return serviceMain(*settings);
    }
    catch (const std::exception& e)
    {
        NX_LOGX(lm("Error starting. %1").arg(e.what()), cl_logERROR);
        return 3;
    }
}

int Service::runMainLoop()
{
    reportStartupResult(true);

    return m_processTerminationEvent.get_future().get();
}

bool Service::isTerminated() const
{
    return m_isTerminated.load();
}

void Service::initializeLog(const AbstractServiceSettings& settings)
{
    auto logSettings = settings.logging();
    logSettings.updateDirectoryIfEmpty(settings.dataDir());
    utils::log::initialize(logSettings, m_applicationDisplayName);
}

void Service::reportStartupResult(bool result)
{
    if (m_startedEventHandler)
    {
        decltype(m_startedEventHandler) startedEventHandler;
        m_startedEventHandler.swap(startedEventHandler);
        startedEventHandler(result);
    }
}

} // namespace utils
} // namespace nx
