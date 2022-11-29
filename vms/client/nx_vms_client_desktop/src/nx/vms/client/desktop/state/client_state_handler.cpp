// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client_state_handler.h"

#include <QtCore/QMap>
#include <QtCore/QPointer>

#include <client/client_runtime_settings.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/uuid.h>
#include <statistics/statistics_manager.h>
#include <ui/statistics/modules/session_restore_statistics_module.h>

#include "client_state_storage.h"
#include "shared_memory_manager.h"

namespace nx::vms::client::desktop {

void ClientStateDelegate::reportStatistics(QString name, QString value)
{
    if (m_reportStatisticsFunction)
        m_reportStatisticsFunction(name, value);
}

void ClientStateDelegate::reportStatistics(QString name, QPoint value)
{
    reportStatistics(name, nx::format("%1,%2", value.x(), value.y()));
}

void ClientStateDelegate::reportStatistics(QString name, QSize value)
{
    reportStatistics(name, nx::format("%1x%2", value.width(), value.height()));
}

void ClientStateDelegate::setReportStatisticsFunction(
    ReportStatisticsFunction reportStatisticsFunction)
{
    m_reportStatisticsFunction = reportStatisticsFunction;
}

struct ClientStateHandler::Private
{
    Private(const QString& storageDirectory):
        clientStateStorage(storageDirectory)
    {
    }

    /** Actual startup parameters, which must be handled after connection complete. */
    StartupParameters startupParameters;

    /** SessionState data that was passed as startup parameter. */
    SessionState sessionState;

    /** Id of the current session. Duplicates the one stored in the shared memory. */
    SessionId sessionId;

    QPointer<SharedMemoryManager> sharedMemoryManager;
    ClientProcessExecutionInterfacePtr processExecutionInterface;
    ClientStateStorage clientStateStorage;
    std::map<QString, ClientStateDelegatePtr> delegates;
    QnSessionRestoreStatisticsModule* statisticsModule = nullptr;
    QnStatisticsManager* statisticsManager = nullptr;
};

ClientStateHandler::ClientStateHandler(const QString& storageDirectory):
    d(new Private(storageDirectory))
{
}

ClientStateHandler::~ClientStateHandler()
{
}

SharedMemoryManager* ClientStateHandler::sharedMemoryManager() const
{
    return d->sharedMemoryManager;
}

void ClientStateHandler::setSharedMemoryManager(SharedMemoryManager* manager)
{
    d->sharedMemoryManager = manager;
}

ClientProcessExecutionInterface* ClientStateHandler::clientProcessExecutionInterface() const
{
    return d->processExecutionInterface.get();
}

void ClientStateHandler::setClientProcessExecutionInterface(
    ClientProcessExecutionInterfacePtr executionInterface)
{
    d->processExecutionInterface = std::move(executionInterface);
}

void ClientStateHandler::registerDelegate(const QString& id, ClientStateDelegatePtr delegate)
{
    if (NX_ASSERT(d->delegates.find(id) == d->delegates.cend(),
        "Duplicate client state delegate id: \"%1\"", id))
    {
        d->delegates[id] = std::move(delegate);
        d->delegates[id]->setReportStatisticsFunction(
            [this](const QString& name, const QString& value)
            {
                if (d->statisticsModule)
                    d->statisticsModule->report(name, value);
            });
    }
}

void ClientStateHandler::unregisterDelegate(const QString& id)
{
    if (d->delegates.contains(id))
        d->delegates[id]->setReportStatisticsFunction({});

    d->delegates.erase(id);
}

void ClientStateHandler::clientStarted(StartupParameters parameters)
{
    if (!qnRuntime->isDesktopMode())
        return;

    d->startupParameters = parameters;

    switch (parameters.mode)
    {
        case StartupParameters::Mode::cleanStartup:
        {
            d->sessionState = d->clientStateStorage.readCommonSubstate();
            break;
        }
        case StartupParameters::Mode::inheritState:
        {
            d->sessionState = d->clientStateStorage.takeTemporaryState(parameters.key);
            break;
        }
        case StartupParameters::Mode::loadSession:
        {
            d->sessionState = d->clientStateStorage.readSessionState(
                parameters.sessionId,
                parameters.key);
            break;
        }
        default:
            break;
    }

    applyState(d->sessionState, ClientStateDelegate::Substate::systemIndependentParameters);
}

void ClientStateHandler::clientClosed()
{
    if (!qnRuntime->isDesktopMode())
        return;

    const auto state = serializeState(
        ClientStateDelegate::Substate::systemIndependentParameters);
    d->clientStateStorage.writeCommonSubstate(state);
}

void ClientStateHandler::clientConnected(bool fullRestoreIsEnabled, SessionId sessionId)
{
    if (!qnRuntime->isDesktopMode())
        return;

    d->sessionId = sessionId;

    switch (d->startupParameters.mode)
    {
        case StartupParameters::Mode::cleanStartup:
        {
            if (fullRestoreIsEnabled)
            {
                const bool sessionIsRunningAlready = NX_ASSERT(d->sharedMemoryManager)
                    && d->sharedMemoryManager->sessionIsRunningAlready(d->sessionId);

                if (!sessionIsRunningAlready && hasSavedConfiguration())
                {
                    restoreWindowsConfiguration();
                    break; //< We are done.
                }
            }

            // Full restore hasn't been executed. Use autosaved state instead.
            applyState(
                d->clientStateStorage.readSystemSubstate(d->sessionId),
                ClientStateDelegate::Substate::systemSpecificParameters);

            break;
        }
        case StartupParameters::Mode::inheritState:
        {
            applyState(d->sessionState, ClientStateDelegate::Substate::systemSpecificParameters);
            break;
        }
        case StartupParameters::Mode::loadSession:
        {
            if (NX_ASSERT(d->startupParameters.sessionId == d->sessionId))
            {
                applyState(
                    d->sessionState,
                    ClientStateDelegate::Substate::systemSpecificParameters);
            }

            break;
        }
        default:
            break;
    }

    if (d->statisticsManager)
        d->statisticsManager->saveCurrentStatistics();
    if (d->statisticsModule)
        d->statisticsModule->reset();
}

void ClientStateHandler::clientDisconnected()
{
    if (!qnRuntime->isDesktopMode())
        return;

    if (!NX_ASSERT(d->sessionId != SessionId()))
        return;

    d->startupParameters = {};
    d->sessionState = {};
    d->sessionId = {};
}

void ClientStateHandler::createNewWindow(bool logIn, const QStringList& args)
{
    SessionState state;
    for (const auto& [delegateId, delegate]: d->delegates)
    {
        DelegateState value;
        delegate->createInheritedState(
            &value,
            logIn
                ? ClientStateDelegate::Substate::allParameters
                : ClientStateDelegate::Substate::systemIndependentParameters,
            {});
        state.insert(delegateId, value);
    }

    const auto temporaryName = QnUuid::createUuid().toString();
    d->clientStateStorage.putTemporaryState(temporaryName, state);

    StartupParameters params{
        .mode = StartupParameters::Mode::inheritState,
        .key = temporaryName
    };
    if (logIn)
    {
        params.sessionId = d->sessionId;
        params.connectionInfo = connectionInfo();
    }
    if (!args.isEmpty())
    {
        const auto& mime = args.first();
        if (logIn)
            params.delayedDrop = mime;
        else
            params.instantDrop = mime;
    }

    d->processExecutionInterface->runClient(params);
}

void ClientStateHandler::saveWindowsConfiguration()
{
    if (!NX_ASSERT(d->sessionId != SessionId()))
        return;

    // Clear existing data.
    deleteWindowsConfiguration();

    // Save own state.
    clientRequestedToSaveState();

    // Save other client instances.
    d->sharedMemoryManager->requestToSaveState();
}

void ClientStateHandler::restoreWindowsConfiguration()
{
    if (!NX_ASSERT(d->sessionId != SessionId()))
        return;

    auto windowStates = d->clientStateStorage.readFullSessionState(d->sessionId);
    if (!NX_ASSERT(!windowStates.empty()))
        return;

    // Take the state for current window from container.
    const auto ownState = windowStates.begin()->second;
    windowStates.erase(windowStates.begin());

    // Collect file names from the state map.
    QStringList stateFilenames;
    for (const auto& [filename, sessionState]: windowStates)
    {
        stateFilenames << filename;
    }

    // Assign states to existing client instances of the current session.
    d->sharedMemoryManager->assignStatesToOtherInstances(&stateFilenames);

    // Start new client instances for unassigned states.
    for (const auto& filename: stateFilenames)
    {
        StartupParameters parameters{
            StartupParameters::Mode::loadSession,
            d->sessionId,
            filename,
            connectionInfo()};

        if (NX_ASSERT(d->processExecutionInterface))
            d->processExecutionInterface->runClient(parameters);
    }

    applyState(ownState, ClientStateDelegate::Substate::allParameters, /*force*/ true);
}

void ClientStateHandler::deleteWindowsConfiguration()
{
    if (!NX_ASSERT(d->sessionId != SessionId()))
        return;

    d->clientStateStorage.clearSession(d->sessionId);
}

bool ClientStateHandler::hasSavedConfiguration() const
{
    if (d->sessionId == SessionId())
        return false;

    const auto windowStates = d->clientStateStorage.readFullSessionState(d->sessionId);
    return !windowStates.empty();
}

void ClientStateHandler::clientRequestedToSaveState()
{
    if (!NX_ASSERT(d->sessionId != SessionId()))
        return;

    const QString randomFilename = QnUuid::createUuid().toString() + ".json";
    const auto state = serializeState(ClientStateDelegate::Substate::allParameters);
    d->clientStateStorage.writeSessionState(d->sessionId, randomFilename, state);
}

void ClientStateHandler::clientRequestedToRestoreState(const QString& filename)
{
    if (!NX_ASSERT(d->sessionId != SessionId()))
        return;

    auto state = d->clientStateStorage.readSessionState(d->sessionId, filename);
    applyState(state, ClientStateDelegate::Substate::allParameters, /*force*/ true);
}

void ClientStateHandler::setStatisticsModules(
    QnStatisticsManager* statisticsManager, QnSessionRestoreStatisticsModule* statisticsModule)
{
    d->statisticsManager = statisticsManager;
    d->statisticsModule = statisticsModule;
}

void ClientStateHandler::storeSystemSpecificState()
{
    const auto state = serializeState(ClientStateDelegate::Substate::systemSpecificParameters);
    d->clientStateStorage.writeSystemSubstate(d->sessionId, state);
}

SessionState ClientStateHandler::serializeState(ClientStateDelegate::SubstateFlags flags) const
{
    SessionState result;
    for (const auto& [delegateId, delegate]: d->delegates)
    {
        DelegateState value;
        delegate->saveState(&value, flags);
        result.insert(delegateId, value);
    }
    return result;
}

void ClientStateHandler::applyState(
    const SessionState& state,
    ClientStateDelegate::SubstateFlags flags,
    bool force)
{
    if (!NX_ASSERT(qnRuntime->isDesktopMode()))
        return;

    if (d->statisticsModule)
        d->statisticsModule->startCapturing(force);

    for (const auto& [delegateId, delegate]: d->delegates)
    {
        // Startup parameters are used to override window geometry.
        // They should be used only on client startup, not on windows restore.
        // Note: d->startupParams is cleaned up after the first disconnect, but that's ok.
        const StartupParameters params = force ? StartupParameters() : d->startupParameters;

        const auto it = state.find(delegateId);
        const DelegateState& delegateState = (it == state.end())
            ? delegate->defaultState()
            : it.value();

        delegate->loadState(delegateState, flags, params);
        if (force)
            delegate->forceLoad();
    }

    if (d->statisticsModule)
        d->statisticsModule->stopCapturing();
}

} // namespace nx::vms::client::desktop
