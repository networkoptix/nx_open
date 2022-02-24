// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QString>

#include "abstract_service_settings.h"
#include "move_only_func.h"
#include "std/future.h"
#include "thread/stoppable.h"
#include "thread/sync_queue.h"

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

    void setEnableLoggingInitialization(bool value);

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

    /**
     * Restarts the process without terminating it. The service should have been started already
     * with a call to exec.
     * NOTE: Causes runMainLoop to return.
     * NOTE: serviceMain will be invoked again after it returns.
     * NOTE: Cannot be called in the same thread as exec, because exec blocks until termination.
     * NOTE: exec does not return if restart is called. pleaseStop must be called to stop the
     * the service.
     * NOTE: exec will return the most recent return code if restart is called.
     */
    void restart();

    std::string applicationDisplayName() const;

protected:
    virtual std::unique_ptr<AbstractServiceSettings> createSettings() = 0;
    /**
     * NOTE: Implemention MUST call Service::runMainLoop if all custom initialization succeeded.
     */
    virtual int serviceMain(const AbstractServiceSettings& settings) = 0;

    int runMainLoop();
    bool isTerminated() const;

private:
    enum class ActionToTake
    {
        restart,
        stop,
    };

    bool m_isLoggingInitializationRequired = true;
    int m_argc = 0;
    char** m_argv = nullptr;
    nx::utils::MoveOnlyFunc<void(bool /*isStarted*/)> m_startedEventHandler;
    nx::utils::MoveOnlyFunc<void(ServiceStartInfo)> m_abnormalTerminationHandler;
    const QString m_applicationDisplayName;
    nx::utils::SyncQueue<ActionToTake> m_processTerminationEvents;
    ActionToTake m_actionToTake;
    std::atomic<bool> m_isTerminated;
    QString m_startInfoFilePath;
    std::unique_ptr<AbstractServiceSettings> m_settings;

    int runServiceMain(const AbstractServiceSettings& settings);

    void initializeLog(const AbstractServiceSettings& settings);
    void reportStartupResult(bool result);

    bool isStartInfoFilePresent() const;
    ServiceStartInfo readStartInfoFile();
    void writeStartInfo();
    void removeStartInfoFile();
};

} // namespace utils
} // namespace nx
