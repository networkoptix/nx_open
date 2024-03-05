// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "service.h"

#include <ctime>
#include <sstream>

#include <QtCore/QFile>

#include "log/log.h"
#include "log/log_initializer.h"
#include "scope_guard.h"

namespace nx {
namespace utils {

QByteArray ServiceStartInfo::serialize() const
{
    std::ostringstream stringStream;
    stringStream << std::chrono::system_clock::to_time_t(startTime);
    return QByteArray::fromStdString(stringStream.str());
}

bool ServiceStartInfo::deserialize(QByteArray data)
{
    if (data.isEmpty())
        return false;

    std::stringstream stringStream(data.toStdString(), std::ios_base::in);
    std::time_t val = 0;
    stringStream >> val;
    startTime = std::chrono::system_clock::from_time_t(val);
    return true;
}

//-------------------------------------------------------------------------------------------------

Service::Service(int argc, char **argv, const QString& applicationDisplayName):
    m_argc(argc),
    m_argv(argv),
    m_applicationDisplayName(applicationDisplayName),
    m_actionToTake(ActionToTake::stop),
    m_isTerminated(false)
{
}

void Service::setEnableLoggingInitialization(bool value)
{
    m_isLoggingInitializationRequired = value;
}

void Service::pleaseStop()
{
    m_isTerminated = true;
    m_processTerminationEvents.push(ActionToTake::stop);
}

void Service::setOnStartedEventHandler(
    nx::utils::MoveOnlyFunc<void(bool /*isStarted*/)> handler)
{
    m_startedEventHandler = std::move(handler);
}

void Service::setOnAbnormalTerminationDetected(
    nx::utils::MoveOnlyFunc<void(ServiceStartInfo)> handler)
{
    m_abnormalTerminationHandler.swap(handler);
}

int Service::exec()
{
    auto triggerOnStartedEventHandlerGuard = nx::utils::makeScopeGuard(
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

        if (m_isLoggingInitializationRequired)
            initializeLog(*settings);

        m_startInfoFilePath = nx::format("%1/%2")
            .args(settings->dataDir(), m_applicationDisplayName.toUtf8().toBase64());

        if (isStartInfoFilePresent())
        {
            NX_ERROR(this, nx::format("Start after crash detected"));
            if (m_abnormalTerminationHandler)
                m_abnormalTerminationHandler(readStartInfoFile());
        }
        writeStartInfo();
        auto startInfoFileGuard = nx::utils::makeScopeGuard([this]() { removeStartInfoFile(); });

        return runServiceMain(*settings);
    }
    catch (const std::exception& e)
    {
        NX_ERROR(this, nx::format("Error starting. %1").arg(e.what()));
        return 3;
    }
}

void Service::restart()
{
    if (m_processTerminationEvents.empty())
        m_processTerminationEvents.push(ActionToTake::restart);
}

std::string Service::applicationDisplayName() const
{
    return m_applicationDisplayName.toStdString();
}

int Service::runMainLoop()
{
    reportStartupResult(true);

    m_actionToTake = m_processTerminationEvents.pop();

    return 0;
}

bool Service::isTerminated() const
{
    return m_isTerminated;
}

int Service::runServiceMain(const AbstractServiceSettings& settings)
{
    int result = 0;

    do
    {
        m_actionToTake = ActionToTake::stop;

        result = serviceMain(settings);

        if (m_actionToTake == ActionToTake::restart)
            NX_INFO(this, "Service will restart. serviceMain returned %1", result);

    } while (m_actionToTake == ActionToTake::restart);

    return result;
}

void Service::initializeLog(const AbstractServiceSettings& settings)
{
    auto logSettings = settings.logging();
    logSettings.updateDirectoryIfEmpty(settings.dataDir() + "/log");
    log::setMainLogger(
        nx::log::buildLogger(
            logSettings,
            m_applicationDisplayName));
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

bool Service::isStartInfoFilePresent() const
{
    return QFile(m_startInfoFilePath).exists();
}

ServiceStartInfo Service::readStartInfoFile()
{
    QFile startInfoFile(m_startInfoFilePath);
    if (!startInfoFile.open(QIODevice::ReadOnly))
        return ServiceStartInfo();

    ServiceStartInfo serviceStartInfo;
    if (serviceStartInfo.deserialize(startInfoFile.readAll()))
        return serviceStartInfo;
    return ServiceStartInfo();
}

void Service::writeStartInfo()
{
    ServiceStartInfo serviceStartInfo;
    serviceStartInfo.startTime = std::chrono::system_clock::now();

    QFile startInfoFile(m_startInfoFilePath);
    if (startInfoFile.open(QIODevice::WriteOnly))
        startInfoFile.write(serviceStartInfo.serialize());
}

void Service::removeStartInfoFile()
{
    QFile startInfoFile(m_startInfoFilePath);
    startInfoFile.remove();
}

} // namespace utils
} // namespace nx
