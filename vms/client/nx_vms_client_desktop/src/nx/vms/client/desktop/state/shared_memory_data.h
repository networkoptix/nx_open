// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

/**@file
 * Shared memory data types and algorithms.
 *
 * A process is a structure that describes meaningful for other client instances properties,
 * including inter-process communication mechanism via commands.
 * A session is a structure that describes a group of such processes.
 */

#include <array>
#include <functional>
#include <ranges>

#include <QtCore/QByteArray>
#include <QtCore/QSet>

#include <nx/reflect/enum_instrument.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/json/qt_core_types.h>

#include "client_process_execution_interface.h"
#include "session_id.h"

namespace nx::vms::client::desktop {

/**
 * A structure that contains a fixed amount of processes and the same amount of sessions.
 * A process as well as a session exists if it has a valid id (pid). The position of each element
 * remains unchanged. Also provides convenient algorithms to perform basic operations.
 * This structure is shared between all the clients using the shared memory mechanism.
 */
class NX_VMS_CLIENT_DESKTOP_API SharedMemoryData
{
public:
    static constexpr int kClientCount = 64;
    static constexpr int kCommandDataSize = 64;
    static constexpr int kTokenSize = 8192;
    static constexpr int kCoudUserNameSize = 255;

    using PidType = ClientProcessExecutionInterface::PidType;
    using ScreenUsageData = QSet<int>;
    using Token = std::string;
    using CloudUserName = std::string;

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
            restoreWindowState,

            /** Logout from cloud. */
            logoutFromCloud,

            /** Update cloud layout resources by fetching the latest data. */
            updateCloudLayouts
        );

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

        CloudUserName cloudUserName = {};

        bool operator==(const Process& other) const = default;
    };

    using Command = Process::Command;

    struct Session
    {
        SessionId id = {};
        Token token;

        bool operator==(const Session& other) const = default;
    };

    struct CloudUserSession
    {
        CloudUserName cloudUserName = {};
        Token refreshToken;

        bool operator==(const CloudUserSession& other) const = default;
    };

    using Processes = std::array<Process, kClientCount>;
    using Sessions = std::array<Session, kClientCount>;
    using CloudUserSessions = std::array<CloudUserSession, kClientCount>;

    Processes processes;
    Sessions sessions;
    CloudUserSessions cloudUserSessions;

public:
    /**
     * Finds a process with any pid.
     */
    const Process* findProcess(PidType pid) const;
    Process* findProcess(PidType pid);

    /**
     * Adds a process.
     * @param pid The pid of the new process.
     * @return Created process or nullptr if there is no free space.
     */
    Process* addProcess(PidType pid);

    /**
    * Finds a session with any session id.
     */
    const Session* findSession(SessionId sessionId) const;
    Session* findSession(SessionId sessionId);

    /**
     * Adds a session.
     * @param pid The session id of the new session.
     * @return Created session or nullptr if there is no free space.
     */
    Session* addSession(SessionId sessionId);

    /**
     * Finds the process session (if the process has a valid session id).
     */
    Session* findProcessSession(PidType pid);
    const Session* findProcessSession(PidType pid) const;

    /**
     * Finds a cloud user data with any cloud user name.
     */
    const CloudUserSession* findCloudUserSession(CloudUserName cloudUserName) const;
    CloudUserSession* findCloudUserSession(CloudUserName cloudUserName);

    /**
     * Add cloud user data.
     */
    CloudUserSession* addCloudUserSession(CloudUserName cloudUserName);

    /**
     * Finds the process session (if the process has a valid session id).
     */
    CloudUserSession* findProcessCloudUserSession(PidType pid);
    const CloudUserSession* findProcessCloudUserSession(PidType pid) const;

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

namespace shared_memory {

using ProcessPredicate = std::function<bool(const SharedMemoryData::Process&)>;
using PipeFilterType = decltype(std::ranges::views::filter(ProcessPredicate{}));

/**
 * Creates a filter to except pids.
 */
NX_VMS_CLIENT_DESKTOP_API PipeFilterType exceptPids(const QSet<SharedMemoryData::PidType>& pids);

/**
 * Creates a filter for all processes within the session.
 */
NX_VMS_CLIENT_DESKTOP_API PipeFilterType withinSession(const SessionId& sessionId);

/**
 * Creates an identity filter (may be used as default).
 */
NX_VMS_CLIENT_DESKTOP_API PipeFilterType all();

} // namespace shared_memory

} // namespace nx::vms::client::desktop
