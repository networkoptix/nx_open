#pragma once

#include <chrono>

#include <QtCore/QString>

#include "abstract_service_settings.h"
#include "move_only_func.h"
#include "std/future.h"
#include "thread/stoppable.h"

namespace nx {
namespace utils {

class ServiceStartInfo
{
public:
    std::chrono::system_clock::time_point startTime;

    QByteArray serialize() const;
    bool deserialize(QByteArray data);
};

class NX_UTILS_API Service:
    public QnStoppable
{
public:
    Service(int argc, char **argv, const QString& applicationDisplayName);
    virtual ~Service() = default;

    virtual void pleaseStop() override;

    void setOnStartedEventHandler(
        nx::utils::MoveOnlyFunc<void(bool /*isStarted*/)> handler);

    /**
     * After starting service check presence of special file in the data directory.
     * If that file is present than this event is reported.
     * After that file is created/re-written with ServiceStartInfo.
     * On correct deinitialization this file is removed.
     */
    void setOnAbnormalTerminationDetected(
        nx::utils::MoveOnlyFunc<void(ServiceStartInfo)> handler);

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
    nx::utils::MoveOnlyFunc<void(ServiceStartInfo)> m_abnormalTerminationHandler;
    const QString m_applicationDisplayName;
    std::atomic<bool> m_isTerminated;
    QString m_startInfoFilePath;

    void initializeLog(const AbstractServiceSettings& settings);
    void reportStartupResult(bool result);

    bool isStartInfoFilePresent() const;
    ServiceStartInfo readStartInfoFile();
    void writeStartInfo();
    void removeStartInfoFile();
};

} // namespace utils
} // namespace nx
