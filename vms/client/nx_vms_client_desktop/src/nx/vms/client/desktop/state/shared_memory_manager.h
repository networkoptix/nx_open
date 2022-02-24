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

namespace nx::vms::client::desktop {

/**
 * This structure is shared between all the clients using the shared memory mechanism.
 */
struct NX_VMS_CLIENT_DESKTOP_API SharedMemoryData
{
    static constexpr int kClientCount = 64;
    static constexpr int kCommandDataSize = 64;
    static constexpr int kTokenSize = 64;

    using PidType = ClientProcessExecutionInterface::PidType;
    using ScreenUsageData = QSet<int>;
    using Token = std::string;

    struct Process
    {
        /** List of all possible commands, which one client can send to others. */
        NX_REFLECTION_ENUM_CLASS_IN_CLASS(Command,
            none,

            /** If the current client modifies local settings, others must reload them. */
            reloadSettings,

            /** Close window without saving its state. */
            exit,

            /** Save full window state for later use during Session Restore. */
            saveWindowState,

            /** Restore window state from file. */
            restoreWindowState);

        /** PID of the process. */
        PidType pid = 0;

        /** List of screens, which are used by the client. 8 bytes. */
        ScreenUsageData usedScreens;

        /** Actual session id if the client is connected to the system. */
        SessionId sessionId = {};

        /** Actual command, which should be executed by the client. */
        Command command = Command::none;

        /**
         * Additional data associated with command. Must not exceed kCommandDataSize bytes.
         *
         * Contains file name for restoreWindowState command.
         * Ignored for other commands.
         */
        QByteArray commandData;

        bool operator==(const Process& other) const = default;
    };

    using Command = Process::Command;

    struct Session
    {
        SessionId id = {};
        Token token;

        bool operator==(const Session& other) const = default;
    };

    using Processes = std::array<Process, kClientCount>;
    using Sessions = std::array<Session, kClientCount>;

    Processes processes;
    Sessions sessions;

    bool operator==(const SharedMemoryData& other) const = default;
};

#define SharedMemoryDataProcess_Fields (pid)(usedScreens)(sessionId)(command)(commandData)
NX_REFLECTION_INSTRUMENT(SharedMemoryData::Process, SharedMemoryDataProcess_Fields);

#define SharedMemoryDataSession_Fields (id)(token)
NX_REFLECTION_INSTRUMENT(SharedMemoryData::Session, SharedMemoryDataSession_Fields);

#define SharedMemoryData_Fields (processes)(sessions)
NX_REFLECTION_INSTRUMENT(SharedMemoryData, SharedMemoryData_Fields);

/** GTest output support. */
NX_VMS_CLIENT_DESKTOP_API void PrintTo(SharedMemoryData::Command command, ::std::ostream* os);
NX_VMS_CLIENT_DESKTOP_API void PrintTo(SharedMemoryData::Process process, ::std::ostream* os);
NX_VMS_CLIENT_DESKTOP_API void PrintTo(SharedMemoryData::Session session, ::std::ostream* os);

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

    /**
     * Index of the current client instance. Clients are receving indices by the start order. If
     * some client was closed, it's index become free and can be taken by any client started after.
     */
    int currentInstanceIndex() const;

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

signals:
    void clientCommandRequested(SharedMemoryData::Command command, const QByteArray& data);
    void sessionTokenChanged(const std::string& token);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
