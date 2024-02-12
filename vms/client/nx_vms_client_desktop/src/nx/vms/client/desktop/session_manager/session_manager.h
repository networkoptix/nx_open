// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <QtCore/QDir>
#include <QtCore/QJsonValue>
#include <QtCore/QSharedMemory>

#include <nx/fusion/serialization/json_functions.h>
#include <nx/utils/singleton.h>
#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop::session {

class ProcessInterface;

struct Config
{
    /**
     * Root GUID.
     * Used as a base for persistent GUID generator across multiple client's processes.
     */
    nx::Uuid rootGuid;
    /** Prefix is used to generate shared memory file. */
    QString sharedPrefix;
    /** Path to state storage. */
    QString storagePath;
};

/**
 * @brief The SessionManager class
 * It manages shared data between multiple client instances
 * 1. Settings. We should notify another clients when we alter settings file
 * 2. Session instance. Session is created/acquired when client connects to a server.
 * 3. Session exit. We should close all clients when one client disconnects from the server.
 * 4. Discover additional states for a session. We need to open additional client for each
 *     session discovered.
 * 5. Persistent GUID generator for multiple client instances. It is used by videowall.
 * ...
 *
 * Session-specific storage is designed for the following usage pattern:
 * 1. User has initiated a new session by connecting to the server.
 * 2. `SessionManager` have loaded session data and emitted `sessionStarted` signal.
 * 3. All subscribed objects read their own data using `SessionManager::read` method. They
 *     can read data inside `sessionStarted` handler or after that (but not after 'sessionEnded').
 * 4. All objects with session-specific data call `SessionManager::write` method to save
 *     data when some relevant client state has changed. This method is thread-safe and does not
 *     block calling thread.
 * 5. User closes the session. All subscribed objects have their last chance to save data inside
 *     `SessionManager::sessionEnded` signal handler. All read/write operations will
 *     cause NX_ASSERT after that.
 *
 * There is only one instance of SessionManager per application. Instance can be
 * obtained either by qnClientModule->instanceManager() or workbenchContext->instanceManager().
 */
class NX_VMS_CLIENT_DESKTOP_API SessionManager: public QObject
{
    Q_OBJECT

public:
    /** Internal state for session storage. */
    enum class SessionState
    {
        /** Not connected to session storage. */
        offline,
        /** Starting session. */
        starting,
        /** Session data is loaded and client can save its state. */
        ready,
        /** Session end is in progress. */
        endingSession,
    };

    enum SessionFlag
    {
        /** Client has acquired a completely new session. */
        NewSession = 1,
        /** There is some data to be restored. */
        HasState = 2,
        /** It is the first client instance to connect to current session. */
        MasterInstance = 4
    };

    Q_DECLARE_FLAGS(SessionFlags, SessionFlag)

    /**
     * Constructor.
     * It will attach to shared state automatically.
     * @param rootInstanceGuid - GUID to be used as base for persistent guid pool.
     * @param sharedPrefix - prefix for shared memory file.
     * @param processInterface - ProcessInterface instance.
     * @param parent - parent object
     */
    SessionManager(
        const Config& config,
        ProcessInterface* processInterface,
        QObject* parent = nullptr);

    virtual ~SessionManager() override;

    /** Sets the function to be used when we need to spawn new client instances. */
    void setProcessSpawner(std::function<void()> spawner);

    /** Enables 'ChildClient' mode for one session. */
    void setChildClientMode();

    /** Get process ID of a current client instance. For debug purposes. */
    int instancePid() const;

    bool isOwnSettingsDirty() const;
    void markOtherSettingsDirty(bool value);
    void markOwnSettingsDirty(bool value);

    bool isSessionEnded() const;

    /** Checks if manager is attached to shared memory instance. */
    bool isValid() const;

    /** Checks if there is an active session. */
    bool isSessionOnline() const;

    /**
     * Enables or disables session restoration.
     */
    void setRestoreUserSessionData(bool value);

    /**
     * Loads session-specific data.
     * Can return false if it fails to get access to session data.
     * @param systemId - system ID we are connecting to.
     * @param userId - user ID we are using.
     * @param forceNewInstance - forces manager find a new instance without any stored data.
     * If forceNewInstance is true, manager can occupy lost session.
     */
    bool acquireSessionInstance(const nx::Uuid& systemId, const nx::Uuid& userId, bool forceNewInstance);

    /**
     * Releases data for acquired instance.
     * @param dropState - should we completely remove this state after exit.
     * @param exitAllClients - should we command other clients to exit.
     */
    void releaseSessionInstance(bool dropState, bool exitAllClients);

    /**
     * Write data to session storage.
     * Actual interaction with storage is done in a background thread.
     * @param key - storage key. Each feature should have its unique key.
     * @param data - data to be stored.
     * @returns true if operation was successful.
     */
    bool write(const QString& key, const QJsonValue& data);

    /**
     * Fusion-powered version of storing data to a session storage.
     * Actual interaction with storage is done in a background thread.
     * @param key - storage key. Each feature should have its unique key.
     * @param data - data to be stored.
     * @returns true if operation was successful.
     */
    template <class Type>
    bool write(const QString& key, const Type& data)
    {
        QJsonValue rawData;
        QJson::serialize(data, &rawData);
        return write(key, rawData);
    }

    /**
     * Read data from the session storage.
     * This data is already in RAM, so this operation should be instant.
     * @param key - storage key. Each feature should have its unique key.
     * @param data - stored data would be copied there.
     * @returns true if operation was successful. Can return false if there was no data for key.
     */
    bool read(const QString& key, QJsonValue& data);

    /**
     * Fusion-powered version of reading data from the session storage.
     * This data is already in RAM, so this operation should be instant.
     * @param key - storage key. Each feature should have its unique key.
     * @param data - stored data would be stored there.
     * @returns true if operation was successful. Can return false if there was no data for key.
     */
    template <class Type>
    bool read(const QString& key, Type& data)
    {
        QJsonValue rawData;
        if (!read(key, rawData))
            return false;
        if (!QJson::deserialize(rawData, &data))
            return false;
        return true;
    }

signals:
    /** Signal is emitted when the settings are changed. */
    void settingsChanged();

    /** Signal is emitted when the current session has started. */
    void sessionStarted(const nx::Uuid& systemId, const nx::Uuid& user, SessionFlags flags);

    /**
     * Signal is emitted when the current session is being closed.
     * It is still possible to save state inside signal handler.
     */
    void sessionEnding(
        const nx::Uuid& systemId,
        const nx::Uuid& user);

    /**
     * Signal is emitted when the current session is closed.
     * No state can be saved inside and after this signal.
     * It can be used to track client's offline status.
     */
    void sessionEnded(
        const nx::Uuid& systemId,
        const nx::Uuid& user);

    /**
     * Signal is emitted when another client has disconnected from the same session.
     */
    void exitRequestedByAnotherClient(
        const nx::Uuid& systemId,
        const nx::Uuid& user);

protected:
    /** Initializes connection to shared memory. */
    bool initializeSharedState();

    /** Checks if client should exit current session. */
    void checkExitSession();

    void atCheckSharedEvents();

    /** Forces saving of all data. */
    bool saveUnsafe(QJsonObject data, bool detach);

    /** This method is started in a separate thread and deals with saving data. */
    void savingThread();

    QDir getSessionRootDirectory() const;
    bool getSessionDirectory(const QString& sessionName, QDir& output) const;

    QSet<int> findRunningStatesUnsafe(const QString& session);

    int spawnOtherClientsUnsafe(const QDir& sessionDir, const QString& sessionName);

private:
    struct InstanceData;
    class SharedMemoryLocker;
    int acquireNewState(QDir storageDir, const QString& sessionName);
    int acquireExistingState(QDir storageDir, const QString& sessionName);

private:
    struct Private;
    QScopedPointer<Private> d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(SessionManager::SessionFlags);

/**
 * ProcessInterface wraps up interaction with system processes.
 * We need a separate mock class to test SessionManager in a single process.
 */
class ProcessInterface
{
public:
    virtual qint64 getOwnPid() const = 0;
    virtual bool checkProcessExists(qint64 pid) const = 0;
    virtual ~ProcessInterface() = default;
};

} // namespace nx::vms::client::desktop::session
