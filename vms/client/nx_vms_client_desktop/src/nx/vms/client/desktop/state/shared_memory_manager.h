// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <array>

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QSet>

#include <nx/reflect/enum_instrument.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/impl_ptr.h>

#include "client_process_execution_interface.h"
#include "session_id.h"
#include "shared_memory_data.h"

namespace nx::vms::client::desktop {


class SharedMemoryInterface
{
public:
    virtual ~SharedMemoryInterface() {};

    virtual bool initialize() = 0;

    virtual void lock() = 0;
    virtual void unlock() = 0;

    virtual SharedMemoryData data() const = 0;
    virtual void setData(const SharedMemoryData& data) = 0;
};
using SharedMemoryInterfacePtr = std::unique_ptr<SharedMemoryInterface>;

/** Controls a shared memory block, used for the IPC between running client instances. */
class NX_VMS_CLIENT_DESKTOP_API SharedMemoryManager: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    /**
     * Instantiate the shared memory manager using the given interface implementation.
     */
    explicit SharedMemoryManager(
        SharedMemoryInterfacePtr memoryInterface,
        ClientProcessExecutionInterfacePtr processInterface,
        QObject* parent = nullptr);
    virtual ~SharedMemoryManager() override;

    /** Sets up the required signal-slot connections. */
    void connectToCloudStatusWatcher();

    /**
     * Index of the current client instance. Clients are receving indices by the start order. If
     * some client was closed, it's index become free and can be taken by any client started after.
     */
    int currentInstanceIndex() const;

    /** Whether current client instance is the only one running. */
    bool isSingleInstance() const;

    /**
     * List of all currently running client instances' indices.
     */
    QList<int> runningInstancesIndices() const;

    /** All screens, used by the client instances on the current PC. */
    SharedMemoryData::ScreenUsageData allUsedScreens() const;

    /** Notify other client instances about screens, used by the current client instance. */
    void setCurrentInstanceScreens(const SharedMemoryData::ScreenUsageData& screens);

    /**
     * Request other client instances of the current session to save state.
     * The current instance wouldn't receive save command.
     */
    void requestToSaveState();

    /**
     * Request other client instances to close window without saving.
     * The current instance wouldn't receive exit command.
     */
    void requestToExit();

    /**
     * Iterates over client instances of the current session and requests them to load state
     * from hard drive. Assigned states are removed from the states list.
     * The current instance woldn't take a state to load and woldn't receive loadState command.
     */
    void assignStatesToOtherInstances(QStringList* windowStates);

    /**
     * Request other client instances to reload their local settings.
     */
    void requestSettingsReload();

    /** Check if there are other clients running the same session. */
    bool sessionIsRunningAlready(const SessionId& sessionId) const;

    /**
     * Check if there are other clients running the same session.
     * This method checks that other instances (if any) are actually alive (i.e. not exited/crashed).
     */
    bool isLastInstanceInCurrentSession() const;

    /** Process queued client command (if any) and emit corresponding signal. */
    void processEvents();

    /** Sets actual session id for the current client instance and creates session if needed. */
    void enterSession(const SessionId& sessionId);

    /**
     * Clears session id for the current client instance and removes session if needed.
     * Returns true if the session is still active.
     */
    bool leaveSession();

    /** Updates token of the current instance session. */
    void updateSessionToken(std::string token);

    /** Updates refresh token for cloud user. */
    void updateRefreshToken(std::string refreshToken);

    /** Notify all instances that they must logout from cloud. */
    void requestLogoutFromCloud();

    /** Update cloud layout resources by fetching the latest data. */
    void requestUpdateCloudLayouts();

signals:
    void clientCommandRequested(SharedMemoryData::Command command, const QByteArray& data);
    void sessionTokenChanged(const std::string& token);
    void refreshTokenChanged(const std::string& token);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
