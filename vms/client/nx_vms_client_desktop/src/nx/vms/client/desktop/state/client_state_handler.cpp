// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client_state_handler.h"

#include <QtCore/QMap>
#include <QtCore/QPointer>

#include <client/client_runtime_settings.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/network/logon_data.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/state/startup_parameters.h>
#include <nx/vms/client/desktop/state/window_geometry_manager.h>
#include <nx/vms/client/desktop/workbench/workbench_ui.h>
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
    if (auto [it, ok] = d->delegates.emplace(id, std::move(delegate));
        NX_ASSERT(ok, "Duplicate client state delegate id: \"%1\"", id))
    {
        it->second->setReportStatisticsFunction(
            [this](const QString& name, const QString& value)
            {
                if (d->statisticsModule)
                    d->statisticsModule->report(name, value);
            });
    }
}

void ClientStateHandler::unregisterDelegate(const QString& id)
{
    if (auto it = d->delegates.find(id); it != d->delegates.end())
    {
        it->second->setReportStatisticsFunction({});
        d->delegates.erase(it);
    }
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

    loadClientState(d->sessionState, ClientStateDelegate::Substate::systemIndependentParameters);
}

void ClientStateHandler::storeSystemIndependentState()
{
    if (!qnRuntime->isDesktopMode())
        return;

    const auto state = serializeState(
        ClientStateDelegate::Substate::systemIndependentParameters);
    d->clientStateStorage.writeCommonSubstate(state);
}

void ClientStateHandler::connectionToSystemEstablished(
    bool fullRestoreIsEnabled,
    const SessionId& sessionId,
    const core::LogonData& logonData)
{
    if (!qnRuntime->isDesktopMode())
        return;

    d->sessionId = sessionId;

    NX_DEBUG(this, "Connection established, session: %1, mode: %2",
        sessionId.toLogString(), (int) d->startupParameters.mode);

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
                    restoreWindowsConfiguration(logonData);
                    break; //< We are done.
                }
            }

            // Full restore hasn't been executed. Use autosaved state instead.
            loadClientState(
                d->clientStateStorage.readSystemSubstate(d->sessionId),
                ClientStateDelegate::Substate::systemSpecificParameters);

            break;
        }
        case StartupParameters::Mode::inheritState:
        {
            loadClientState(
                d->sessionState,
                ClientStateDelegate::Substate::systemSpecificParameters);
            break;
        }
        case StartupParameters::Mode::loadSession:
        {
            if (NX_ASSERT(d->startupParameters.sessionId == d->sessionId))
            {
                loadClientState(
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

    if (!NX_ASSERT(d->sessionId))
        return;

    NX_DEBUG(this, "Client disconnected, session: %1", d->sessionId.toLogString());

    d->startupParameters = {};
    d->sessionState = {};
    d->sessionId = {};
}

void ClientStateHandler::createNewWindow(
    const std::optional<core::LogonData>& logonData,
    const QStringList& args)
{
    SessionState state;
    for (const auto& [delegateId, delegate]: d->delegates)
    {
        DelegateState value;
        delegate->createInheritedState(
            &value,
            logonData
                ? ClientStateDelegate::Substate::allParameters
                : ClientStateDelegate::Substate::systemIndependentParameters,
            {});
        state.insert(delegateId, value);
    }

    const auto temporaryName = nx::Uuid::createUuid().toString(QUuid::WithBraces);
    d->clientStateStorage.putTemporaryState(temporaryName, state);

    StartupParameters params{
        .mode = StartupParameters::Mode::inheritState,
        .key = temporaryName
    };

    if (logonData)
    {
        params.logonData = *logonData;
        params.sessionId = d->sessionId;
    }

    if (!args.isEmpty())
    {
        const auto& mime = args.first();
        if (logonData)
            params.delayedDrop = mime;
        else
            params.instantDrop = mime;
    }

    d->processExecutionInterface->runClient(params);
}

void ClientStateHandler::saveWindowsConfiguration()
{
    if (!NX_ASSERT(d->sessionId))
        return;

    // Clear existing data.
    deleteWindowsConfiguration();

    // Save own state.
    clientRequestedToSaveState();

    // Save other client instances.
    d->sharedMemoryManager->requestToSaveState();
}

void ClientStateHandler::restoreWindowsConfiguration(const core::LogonData& logonData)
{
    if (!NX_ASSERT(d->sessionId))
        return;

    auto windowStates = d->clientStateStorage.readFullSessionState(d->sessionId);
    if (!NX_ASSERT(!windowStates.empty()))
        return;

    // Take the state for current window from container.
    auto ownState = windowStates.begin()->second;
    windowStates.erase(windowStates.begin());

    auto commonState = d->clientStateStorage.readCommonSubstate();
    if (ownState.value(WorkbenchUi::kWorkbenchUiDataKey).empty())
    {
        ownState.insert(
            WorkbenchUi::kWorkbenchUiDataKey,
            commonState.value(WorkbenchUi::kWorkbenchUiDataKey));
    }
    if (ownState.value(WindowGeometryManager::kWindowGeometryData).empty())
    {
        ownState.insert(
            WindowGeometryManager::kWindowGeometryData,
            commonState.value(WindowGeometryManager::kWindowGeometryData));
    }

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
            .mode = StartupParameters::Mode::loadSession,
            .sessionId = d->sessionId,
            .key = filename,
            .logonData = logonData};

        if (NX_ASSERT(d->processExecutionInterface))
            d->processExecutionInterface->runClient(parameters);
    }
    loadClientState(ownState);
}

void ClientStateHandler::deleteWindowsConfiguration()
{
    if (!NX_ASSERT(d->sessionId))
        return;

    d->clientStateStorage.clearSession(d->sessionId);
}

bool ClientStateHandler::hasSavedConfiguration() const
{
    if (!d->sessionId)
        return false;

    const auto windowStates = d->clientStateStorage.readFullSessionState(d->sessionId);
    return !windowStates.empty();
}

void ClientStateHandler::clientRequestedToSaveState()
{
    if (!NX_ASSERT(d->sessionId))
        return;

    const QString randomFilename = nx::Uuid::createUuid().toSimpleString() + ".json";
    const auto state = serializeState(ClientStateDelegate::Substate::systemSpecificParameters);
    d->clientStateStorage.writeSessionState(d->sessionId, randomFilename, state);
    storeSystemIndependentState();
}

void ClientStateHandler::clientRequestedToRestoreState(const QString& filename)
{
    if (!NX_ASSERT(d->sessionId))
        return;

    loadClientState(d->clientStateStorage.readSessionState(d->sessionId, filename));
}

void ClientStateHandler::setStatisticsModules(
    QnStatisticsManager* statisticsManager, QnSessionRestoreStatisticsModule* statisticsModule)
{
    d->statisticsManager = statisticsManager;
    d->statisticsModule = statisticsModule;
}

void ClientStateHandler::storeSystemSpecificState()
{
    if (!NX_ASSERT(d->sessionId))
        return;

    NX_VERBOSE(this, "Store system-specific state, session: %1", d->sessionId.toLogString());

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

void ClientStateHandler::loadClientState(
    const SessionState& state,
    ClientStateDelegate::SubstateFlags flags,
    bool applyState)
{
    NX_VERBOSE(this, "Load client state, flags: %1, apply: %2", flags, applyState);

    if (!NX_ASSERT(qnRuntime->isDesktopMode()))
        return;

    if (d->statisticsModule)
        d->statisticsModule->startCapturing(applyState);

    for (const auto& [delegateId, delegate]: d->delegates)
    {
        // Startup parameters are used to override window geometry.
        // They should be used only on client startup, not on windows restore.
        // Note: d->startupParams is cleaned up after the first disconnect, but that's ok.
        const StartupParameters params = applyState ? StartupParameters() : d->startupParameters;

        const auto it = state.find(delegateId);
        const DelegateState& delegateState = (it == state.end())
            ? delegate->defaultState()
            : it.value();

        delegate->loadState(delegateState, flags, params);
        if (applyState)
            delegate->forceLoad();
    }

    if (d->statisticsModule)
        d->statisticsModule->stopCapturing();
}

void ClientStateHandler::loadClientState(const SessionState& state)
{
    if (ini().enableMultiSystemTabBar)
    {
        loadClientState(
            state,
            ClientStateDelegate::Substate::systemSpecificParameters,
            /*force*/ true);
        loadClientState(
            d->clientStateStorage.readCommonSubstate(),
            ClientStateDelegate::Substate::systemIndependentParameters,
            /*force*/ true);
    }
    else
    {
        loadClientState(state, ClientStateDelegate::Substate::allParameters, /*force*/ true);
    }
}

} // namespace nx::vms::client::desktop
