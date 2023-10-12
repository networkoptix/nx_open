// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QJsonValue>
#include <QtCore/QString>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/network/logon_data.h>

#include "client_process_execution_interface.h"
#include "session_id.h"
#include "session_state.h"
#include "startup_parameters.h"

class QnStatisticsManager;
class QnSessionRestoreStatisticsModule;

namespace nx::vms::client::core { struct ConnectionInfo; }

namespace nx::vms::client::desktop {

class WindowGeometryManager;
class SharedMemoryManager;

class ClientStateDelegate
{
public:
    enum class Substate
    {
        systemIndependentParameters = 1,
        systemSpecificParameters = 2,
        allParameters = systemIndependentParameters | systemSpecificParameters
    };
    Q_DECLARE_FLAGS(SubstateFlags, Substate)

    using ReportStatisticsFunction = std::function<void(const QString&, const QString&)>;

    virtual bool loadState(
        const DelegateState& state,
        SubstateFlags flags,
        const StartupParameters& params) = 0;
    virtual void saveState(DelegateState* state, SubstateFlags flags) = 0;

    /** Create inherited state for a New Window. */
    virtual void createInheritedState(
        DelegateState* state,
        SubstateFlags flags,
        const QStringList& /*resources*/)
    {
        saveState(state, flags);
    }

    virtual DelegateState defaultState() const { return {}; }

    /**
     * Force update of state for delegates with delayed processing (e.g. Workbench delegate).
     * This method may be removed when clientConnected() would be called after full initialization.
     */
    virtual void forceLoad() {}

    template<typename T>
    void reportStatistics(QString name, const T& value)
    {
        reportStatistics(name, QString::fromStdString(nx::reflect::toString(value)));
    }

    void reportStatistics(QString name, QString value);
    void reportStatistics(QString name, QPoint value);
    void reportStatistics(QString name, QSize value);

    void setReportStatisticsFunction(ReportStatisticsFunction reportStatisticsFunction);

    virtual ~ClientStateDelegate() {}

private:
    ReportStatisticsFunction m_reportStatisticsFunction;
};
using ClientStateDelegatePtr = std::shared_ptr<ClientStateDelegate>;

Q_DECLARE_OPERATORS_FOR_FLAGS(ClientStateDelegate::SubstateFlags)

class NX_VMS_CLIENT_DESKTOP_API ClientStateHandler
{
public:
    explicit ClientStateHandler(const QString& storageDirectory);
    virtual ~ClientStateHandler();

    /** Actual shared memory manager. */
    SharedMemoryManager* sharedMemoryManager() const;

    /** Enable IPC using the provided manager. */
    void setSharedMemoryManager(SharedMemoryManager* manager);

    /** Actual process execution interface. */
    ClientProcessExecutionInterface* clientProcessExecutionInterface() const;

    /** Set interface for running new client instances. */
    void setClientProcessExecutionInterface(ClientProcessExecutionInterfacePtr executionInterface);

    /**
     * Add delegate for the stateful element.
     */
    void registerDelegate(const QString& id, ClientStateDelegatePtr delegate);

    /**
     * Remove previously registered delegate.
     */
    void unregisterDelegate(const QString& id);

    /**
     * Handle client start.
     *
     * If the session file was provided as a startup parameter, client must load system-independent
     * substate from this file and wait for connection to be processes. After connect client must
     * load system-specific parameters. If client was run without additional parameters, common
     * system-independent state is used for initialization.
     */
    void clientStarted(StartupParameters parameters);

    /**
     * Common system-independent state will be updated.
     */
    void storeSystemIndependentState();

    /**
     * Handle client connection.
     *
     * If the client was run without additional parameters, it must read contents of the session
     * folder. If this folder contains stored window(s) configuration, full restore is enabled and
     * there are no other client instances in current session, then full restore is performed (both
     * system-independent and system-specific parameters are loaded for this window, additional
     * client instances are started if necessary). Otherwise only system-specific parameters are
     * loaded from autosave file. If there is no saved state, default values are used.
     *
     * If the client was run with the state provided, it must apply system-specific parameters from
     * this state.
     *
     * @param logonData Logon data to run new client instances to connect to the same System.
     */
    void connectionToSystemEstablished(
        bool fullRestoreIsEnabled,
        SessionId sessionId,
        core::LogonData logonData);

    /**
     * Handle state saving on explicit disconnect. System-specific state will be updated.
     */
    void clientDisconnected();

    /**
     * Starts new Client instance, whose state is derived from the current client state.
     *
     * @param logonData If set, the new instance will connect to the same system using existing
     * credentials. Otherwise, new WelcomeScreen would be opened.
     * @param args Specifies additional arguments used for the new state generation. Right now
     * may contain only one item with serialized MimeData.
     */
    void createNewWindow(
        std::optional<core::LogonData> logonData = std::nullopt,
        const QStringList& args = {});

    /**
     * Save window(s) configuration of the current session.
     */
    void saveWindowsConfiguration();

    /**
     * Restore previously saved window(s) configuration: start new clients if necessary,
     * request existing client instances in the current session to update their state.
     *
     * @param logonData Logon data to run new client instances to connect to the same System.
     */
    void restoreWindowsConfiguration(core::LogonData logonData);

    /**
     * Delete window(s) configuration of the current session.
     */
    void deleteWindowsConfiguration();

    /**
     * Check if there is a saved window(s) configuration for the current session.
     */
    bool hasSavedConfiguration() const;

    /**
     * Handle state saving that is requested in another instance of client.
     */
    void clientRequestedToSaveState();

    /**
     * Handle state restoring that is requested in another instance of client.
     */
    void clientRequestedToRestoreState(const QString& filename);

    /**
     * Set statistics modules.
     */
    void setStatisticsModules(
        QnStatisticsManager* statisticsManager,
        QnSessionRestoreStatisticsModule* statisticsModule);

    /**
     * System-specific state will be updated.
     */
    void storeSystemSpecificState();

private:
    SessionState serializeState(ClientStateDelegate::SubstateFlags flags) const;
    void loadClientState(
        const SessionState& state,
        ClientStateDelegate::SubstateFlags flags,
        bool applyState = false);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
