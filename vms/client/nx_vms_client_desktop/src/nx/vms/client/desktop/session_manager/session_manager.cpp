// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "session_manager.h"

#include <chrono>
#include <condition_variable>
#include <thread>

#include <QtCore/QCoreApplication>
#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QRegularExpression>
#include <QtCore/QStandardPaths>
#include <QtCore/QTimer>

#include <nx/utils/file_system.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>

namespace {

const int dataVersion = 3;
const int kMaxClients = 256;
const auto kCheckDeadsInterval = std::chrono::seconds(60);
const auto kCheckEventsInterval = std::chrono::seconds(1);
const int kInvalidIndex = -1;
const QString kSessionStateKey = "state";
const int kMaxInstancesPerSession = 20;
constexpr int stateLockStaleTimeMs = 2000;
constexpr int trylockTimeoutMs = 10;

QString makeSessionName(const nx::Uuid& systemId, const nx::Uuid& user)
{
    return QString("%1@%2").arg(user.toSimpleString(), systemId.toSimpleString());
}

} // anonymous namespace

namespace nx::vms::client::desktop::session {

/**
 * Array of these structures is stored in a shared memory.
 * Each active client gets its own InstanceData.
 */
struct SessionManager::InstanceData
{
    qint64 pid = 0;
    int sessionStateIndex = -1;
    // Storage for instance guid in binary form, according to rfc4122
    char instanceId[nx::Uuid::RFC4122_SIZE];

    static constexpr auto kSessionNameSize = 128;
    // Each guid looks like xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx and takes 36 symbols.
    char sessionName[kSessionNameSize];

    bool settingsDirty = false;
    bool exitSession = false;

    // Checks session name.
    bool checkName(const QString& name) const
    {
        int maxLen = static_cast<int>(strnlen(sessionName, kSessionNameSize));
        return name == QLatin1String(sessionName, maxLen);
    }

    void setGuid(const nx::Uuid& uuid)
    {
        auto rawGuid = uuid.toRfc4122();
        NX_ASSERT(rawGuid.size() == nx::Uuid::RFC4122_SIZE);
        memcpy(instanceId, rawGuid.data(), nx::Uuid::RFC4122_SIZE);
    }

    // Sets session name. If name is empty, then sessionName is completely cleaned.
    void setSessionName(const QString& name)
    {
        NX_ASSERT(name.length() < kSessionNameSize);
        if (name.isEmpty())
        {
            memset(sessionName, 0, kSessionNameSize);
        }
        else
        {
            auto rawString = name.toLatin1();
            size_t maxLen = std::min<size_t>(rawString.size() + 1, kSessionNameSize);
            memcpy(sessionName, rawString.data(), maxLen);
        }
    }
};

class SessionManager::SharedMemoryLocker
{
public:
    SharedMemoryLocker(QSharedMemory *memory):
        m_memory(memory),
        m_locked(memory->lock())
    {
        NX_ASSERT(m_locked, "Could not initialize shared memory");
        if (!m_locked)
            qWarning() << "Could not initialize shared memory";
    }

    ~SharedMemoryLocker()
    {
        if (!m_locked)
            return;
        m_memory->unlock();
    }

    bool isValid() const
    {
        return m_locked;
    }

    SessionManager::InstanceData* data(int index = 0)
    {
        return reinterpret_cast<SessionManager::InstanceData*>(m_memory->data()) + index;
    }

private:
    QSharedMemory* m_memory;
    const bool m_locked;
};

struct SessionManager::Private
{
    mutable QSharedMemory sharedMemory;
    /** Own index inside shared storage. */
    int index = kInvalidIndex;
    /**
     * This is root instance GUID. It is often set to hardware PC UUID.
     * Actual instance ID is calculated using this root ID and client's index.
     */
    nx::Uuid rootInstanceGuid = nx::Uuid::createUuid();
    /** Base path to store session data. It is set from config in SessionManager constructor. */
    QString storageBasePath;
    nx::Uuid systemId;
    nx::Uuid userId;
    std::atomic<bool> hasSessionData = false;
    bool exitSession = false;
    /**
     * Setting to enable state restoration. Setting it to 'false' will cause sessionStarted
     * signal to have no SessionFlag::hasState flag. So clients can ignore the data from
     * session storage.
     */
    bool enableSessionRestore = true;
    /** Setting to enable removal of lost states when we try to load existing one. */
    bool removeLostStatesOnStart = false;
    QString instanceFileName;
    bool childClientMode = false;

    /** Method to be used to spawn another client process. */
    std::function<void ()> clientSpawner;
    /** Stores current session data. */
    QJsonObject instanceData;

    QRegularExpression stateFileMatcher{"state([0-9]+)\\.json"};

    /** Revision for session state. It is incremented every time state changes. */
    std::atomic_bool dataChanged = false;
    std::condition_variable saveCondition;
    mutable std::mutex guard;
    std::thread savingThread;

    std::atomic<SessionState> state = SessionState::offline;
    ProcessInterface* processInterface = nullptr;

    Private(const QString& memoryKey): sharedMemory(memoryKey) {}

    QString sessionName() const
    {
        return makeSessionName(systemId, userId);
    }

    QString sharedKey() const
    {
        return sharedMemory.key();
    }

    /** Extracts index from the name of state file. */
    int stateToIndex(const QString& stateFile) const
    {
        std::scoped_lock<decltype(guard)> lock(guard);
        auto match = stateFileMatcher.match(stateFile);
        if (!match.hasMatch())
            return kInvalidIndex;
        auto rawStr = match.captured(1);
        bool ok = false;
        int result = rawStr.toInt(&ok);
        if (!ok)
            return kInvalidIndex;
        return result;
    }

    void clearSessionData()
    {
        hasSessionData = false;
        systemId = {};
        userId = {};
        instanceFileName.clear();
        instanceData = {};
    }

    /** Lists all state files that are saved on the disk. */
    QSet<QString> listSavedStateFiles(QDir sessionDir) const
    {
        sessionDir.setNameFilters({"*.json"});
        QSet<QString> result;
        auto filters = QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks;
        for (auto path: sessionDir.entryList(filters, QDir::SortFlag::Name))
        {
            if (stateToIndex(path) != kInvalidIndex)
                result.insert(path);
        }
        return result;
    }

    /** Converts a set of state names to state indexes. */
    QSet<int> stateListToIndexes(const QSet<QString>& states)
    {
        QSet<int> result;
        for (auto state: states)
            result.insert(stateToIndex(state));
        return result;
    }
};

SessionManager::SessionManager(
    const Config& config,
    ProcessInterface* processInterface,
    QObject *parent):
    QObject(parent)
{
    NX_CRITICAL(processInterface);

    QString sharedStoragePath = config.sharedPrefix + "/" + QString::number(dataVersion);
    d.reset(new Private(sharedStoragePath));
    d->processInterface = processInterface;
    d->storageBasePath = config.storagePath;
    d->rootInstanceGuid = config.rootGuid;

    if (!initializeSharedState())
        NX_ASSERT(false);

    QTimer *checkExitTimer = new QTimer(this);
    connect(checkExitTimer, &QTimer::timeout, this, &SessionManager::atCheckSharedEvents);
    checkExitTimer->start(kCheckEventsInterval);
}

SessionManager::~SessionManager()
{
    if (d->index == -1)
        return;

    if (d->savingThread.joinable())
    {
        NX_VERBOSE(this, "~SessionManager() - stopping internal thread");
        d->state = SessionState::offline;
        d->saveCondition.notify_one();
        d->savingThread.join();
    }

    // Note: We are not saving current data anyhow. It should be done explicitly by
    // calling SessionManager::releaseSessionInstance.

    SharedMemoryLocker lock(&d->sharedMemory);
    if (!lock.isValid())
    {
        NX_ERROR(this, "~SessionManager() - can not acquire shared lock to \"%1\"",
            d->sharedKey());
        return;
    }

    InstanceData *data = lock.data();
    data[d->index].pid = 0;
    data[d->index].settingsDirty = false;
    data[d->index].exitSession = false;
    d->index = kInvalidIndex;
}

bool SessionManager::initializeSharedState()
{
    auto& memory = d->sharedMemory;
    bool success = memory.create(kMaxClients * sizeof(InstanceData));
    if (success)
    {
        SharedMemoryLocker lock(&memory);
        success = lock.isValid();
        if (success)
        {
            InstanceData* instances = lock.data();
            NX_DEBUG(this, "Initializing shared memory");
            // There are non-arbitrary default values inside each instance, so I use placement new
            // instead of a plain memset.
            for (int i = 0; i < kMaxClients; ++i)
                new (instances+i) InstanceData();
        }
    }

    if (!success && memory.error() == QSharedMemory::AlreadyExists)
        success = memory.attach();

    NX_ASSERT(success, "Could not acquire shared memory");
    if (!success)
    {
        NX_ERROR(this, "initializeSharedState() - could not acquire shared memory");
        return false;
    }
    return true;
}

void SessionManager::setProcessSpawner(std::function<void ()> spawner)
{
    d->clientSpawner = spawner;
}

void SessionManager::setChildClientMode()
{
    d->childClientMode = true;
}

int SessionManager::instancePid() const
{
    return d->processInterface->getOwnPid();
}

bool SessionManager::isOwnSettingsDirty() const
{
    if (!isValid())
        return false;

    SharedMemoryLocker lock(&d->sharedMemory);
    if (!lock.isValid())
        return false;

    InstanceData* data = lock.data();
    return data[d->index].settingsDirty;
}

void SessionManager::markOtherSettingsDirty(bool value)
{
    if (!isValid())
        return;

    SharedMemoryLocker lock(&d->sharedMemory);
    if (!lock.isValid())
        return;

    InstanceData* data = lock.data();
    for (int i = 0; i < kMaxClients; i++)
    {
        if (i == d->index)
            continue;

        if (data[i].pid == 0)
            continue;

        data[i].settingsDirty = value;
    }
}

void SessionManager::markOwnSettingsDirty(bool value)
{
    if (!isValid())
        return;

    SharedMemoryLocker lock(&d->sharedMemory);
    if (!lock.isValid())
        return;

    InstanceData* data = lock.data();
    data[d->index].settingsDirty = value;
}

bool SessionManager::isValid() const
{
    return d->index >= 0;
}

bool SessionManager::isSessionOnline() const
{
    return d->hasSessionData;
}

void SessionManager::checkExitSession()
{
    if (!d->hasSessionData || d->index == kInvalidIndex)
        return;

    bool shouldExit = false;
    {
        SharedMemoryLocker lock(&d->sharedMemory);
        if (!lock.isValid())
            return;

        QString sessionName = d->sessionName();
        InstanceData* instance = lock.data(d->index);
        shouldExit = instance->exitSession;
    }

    if (shouldExit)
    {
        NX_DEBUG(this, "checkExitSession() - client should exit.");
        emit exitRequestedByAnotherClient(d->systemId, d->userId);
    }
}

void SessionManager::savingThread()
{
    while(d->state != SessionState::offline)
    {
        std::unique_lock<decltype(d->guard)> lock(d->guard);
        if (d->state != SessionState::starting)
        {
            if (d->dataChanged)
            {
                auto timeStart = std::chrono::steady_clock::now();
                saveUnsafe(d->instanceData, false);
                auto timeDelta = std::chrono::steady_clock::now() - timeStart;
                auto timeDeltaMs = std::chrono::duration_cast<std::chrono::milliseconds>(timeDelta);
                NX_VERBOSE(this, "savingThread() - saved data in %1ms", timeDeltaMs.count());
                d->dataChanged = false;
            }
            if (d->state == SessionState::endingSession)
                break;
        }
        d->saveCondition.wait_for(lock, std::chrono::milliseconds(500));
    }
    NX_DEBUG(this, "savingThread() - process has stopped");
}

void SessionManager::atCheckSharedEvents()
{
    checkExitSession();
}

void SessionManager::setRestoreUserSessionData(bool value)
{
    d->enableSessionRestore = value;
}

bool SessionManager::isSessionEnded() const
{
    return d->exitSession;
}

QSet<int> SessionManager::findRunningStatesUnsafe(const QString& sessionName)
{
    QSet<int> existingSessions;
    InstanceData* instances = reinterpret_cast<InstanceData*>(d->sharedMemory.data());
    for (int i = 0; i < kMaxClients; i++)
    {
        if (i == d->index)
            continue;
        if (!instances[i].checkName(sessionName))
            continue;
        if (!d->processInterface->checkProcessExists(instances[i].pid))
            continue;
        existingSessions.insert(instances[i].sessionStateIndex);
    }
    return existingSessions;
}

bool SessionManager::acquireSessionInstance(
    const nx::Uuid& systemId, const nx::Uuid& userId, bool forceNewInstance)
{
    NX_ASSERT(!userId.isNull());
    NX_ASSERT(!systemId.isNull());
    NX_ASSERT(!d->hasSessionData);
    /*
     * Rules for acquiring a session:
     * 1. Client with special command line parameter --restore tries to load an existing state.
     * 2. First client (there are no other client in this session) without special flag tries to
     *     load an existing state and starts another clients for each additional state discovered.
     * 3. NotFirst client (there is at least one client in this session) always creates the new
     *     state.
     * 4. We should create a new state if a client is unable to get existing state (Case 1 and 2)
     */

    QString sessionName = makeSessionName(systemId, userId);
    QDir sessionDir;
    if (!getSessionDirectory(sessionName, sessionDir))
    {
        NX_ERROR(this, "acquireSessionInstance(%1) - can not get storage directory", sessionName);
        d->hasSessionData = false;
        return false;
    }
    else
    {
        NX_VERBOSE(this, "acquireSessionInstance(%1) - trying to get session file from %2", sessionName, sessionDir.path());
    }

    int stateIndex = kInvalidIndex;
    {
        SharedMemoryLocker lock(&d->sharedMemory);
        if (!lock.isValid())
            return false;

        if (!forceNewInstance)
            stateIndex = acquireExistingState(sessionDir, sessionName);
        if (stateIndex == kInvalidIndex)
            stateIndex = acquireNewState(sessionDir, sessionName);

        InstanceData* instance = lock.data(d->index);
        instance->setSessionName(sessionName);
        instance->sessionStateIndex = stateIndex;
        spawnOtherClientsUnsafe(sessionDir, sessionName);
    }

    if (stateIndex == kInvalidIndex)
    {
        NX_ERROR(this, "acquireSessionInstance() - failed to acquire session state.");
        return false;
    }
    d->userId = userId;
    d->systemId = systemId;
    d->hasSessionData = true;
    d->exitSession = false;
    d->instanceData[kSessionStateKey] = "open";
    d->instanceData["pid"] = d->processInterface->getOwnPid();
    saveUnsafe(d->instanceData, false);

    d->state = SessionState::starting;
    d->savingThread = std::thread(
        [this]()
        {
            savingThread();
        });
    SessionFlags flags;
    if (!d->instanceData.empty() && d->enableSessionRestore)
        flags |= SessionFlag::HasState;
    emit sessionStarted(systemId, userId, flags);
    d->state = SessionState::ready;
    return true;
}

int SessionManager::acquireNewState(QDir sessionDir, const QString& sessionName)
{
    // NOTE: We expect shared memory file is locked.
    // Contains instance indexes for all existing clients belonging to this session.
    QSet<int> existingSessions = findRunningStatesUnsafe(sessionName);

    // We need to get either new data, or load existing data.
    // Iterate all over the files and try to pick up suitable session data.
    for (int i = 0; i < kMaxInstancesPerSession; i++)
    {
        if (existingSessions.contains(i))
            continue;
        QString path = sessionDir.absoluteFilePath(QString("state%1.json").arg(QString::number(i)));
        QFile instanceFile = {path};
        // Now we have session index and we can try to read data from it.
        if (!instanceFile.open(QIODevice::ReadWrite))
        {
            NX_ERROR(this, "Failed to open session storage %1", path);
            continue;
        }

        NX_VERBOSE(this, "Trying to use %1 state %2", sessionName, QString::number(i));
        QByteArray data = instanceFile.readAll();
        instanceFile.close();
        QJsonDocument document = QJsonDocument::fromJson(data);

        if (!document.isEmpty())
            continue;

        d->instanceFileName = path;
        return i;
    }

    return kInvalidIndex;
}

int SessionManager::acquireExistingState(QDir sessionDir, const QString& sessionName)
{
    auto existingStateFiles = d->listSavedStateFiles(sessionDir);
    auto activeStateIndices = findRunningStatesUnsafe(sessionName);
    // We need to get either new data, or load existing data.
    // Iterate all over the files and try to pick up suitable session data.
    for (auto path: existingStateFiles)
    {
        QString fullPath = sessionDir.absoluteFilePath(path);
        int stateIndex = d->stateToIndex(path);

        if (activeStateIndices.contains(stateIndex))
        {
            NX_VERBOSE(this, "acquireExistingState() state index %1 is already acquired", stateIndex);
            continue;
        }

        QFile instanceFile = {fullPath};
        // Now we have session index and we can try to read data from it.
        if (!instanceFile.open(QIODevice::ReadWrite))
        {
            NX_ERROR(this, "acquireExistingState() - Failed to open session storage %1", path);
            continue;
        }

        QByteArray data = instanceFile.readAll();
        instanceFile.close();
        QJsonDocument document = QJsonDocument::fromJson(data);

        if (document.isEmpty())
        {
            NX_ERROR(this, "acquireExistingState() - %1 is empty", path);
            continue;
        }

        QJsonObject root = document.object();
        // TODO: Check the spec: should we track 'lost' sessions?
        // If we should - then we should just erase this file.
        if (root.contains(kSessionStateKey) && d->removeLostStatesOnStart)
        {
            QJsonValue state = root[kSessionStateKey];
            if (state.toString() == "open")
            {
                NX_VERBOSE(this, "acquireExistingState() - removing lost file %1", path);
                instanceFile.remove();
                continue;
            }
        }
        NX_VERBOSE(this, "acquireExistingState() - picked state %1", path);
        d->instanceData = root;
        d->instanceFileName = fullPath;
        return stateIndex;
    }

    return kInvalidIndex;
}

int SessionManager::spawnOtherClientsUnsafe(const QDir& sessionDir, const QString& sessionName)
{
    QSet<int> existingStateIndices = d->stateListToIndexes(d->listSavedStateFiles(sessionDir));
    QSet<int> occupiedStates = findRunningStatesUnsafe(sessionName);

    if (occupiedStates.empty() && d->enableSessionRestore)
    {
        // This client is the first one. It should spawn child processes.
        QSet<int> statesToRestore = existingStateIndices - occupiedStates;
        if (statesToRestore.size() <= 1)
        {
            NX_DEBUG(this, "acquireSessionInstance() - no additional states to restore.");
        }
        else
        {
            NX_DEBUG(this, "acquireSessionInstance() - there are %1 states to restore.",
                statesToRestore.size());
            if (d->clientSpawner)
            {
                // We do not care about particular loading order.
                for ([[maybe_unused]] int i = 0; i < statesToRestore.size() - 1; i++)
                    d->clientSpawner();
                return statesToRestore.size() - 1;
            }
            else
            {
                NX_ERROR(this, "acquireSessionInstance() - there are some states, "
                    "but no clientSpawner specified.");
            }
        }
    }
    return 0;
}

void SessionManager::releaseSessionInstance(bool dropState, bool exitAllClients)
{
    SharedMemoryLocker lock(&d->sharedMemory);
    if (!lock.isValid())
        return;
    if (!d->hasSessionData)
        return;

    QString sessionName = d->sessionName();

    NX_VERBOSE(this, "releaseSessionInstance()");
    if (exitAllClients)
    {
        // Making all other clients with the same session exit as well.
        InstanceData* instances = lock.data();
        for (int i = 0; i < kMaxClients; i++)
        {
            if (!instances[i].checkName(sessionName))
                continue;
            if (!d->processInterface->checkProcessExists(instances[i].pid))
                continue;
            if (i == d->index)
                continue;
            NX_VERBOSE(this, "releaseSessionInstance() - instance=%1 will be closed as well.", i);
            instances[i].exitSession = true;
        }
    }

    InstanceData* instance = lock.data(d->index);
    instance->exitSession = false;
    // Stopping background thread.
    d->state = SessionState::endingSession;
    d->saveCondition.notify_one();
    d->savingThread.join();

    auto systemId = d->systemId;
    auto userId = d->userId;
    // NOTE: There can be some calls to SessionManager::write in callback handlers.
    emit sessionEnding(systemId, userId);
    if (dropState)
        QFile::remove(d->instanceFileName);
    else
        saveUnsafe(d->instanceData, /*detach=*/true);

    d->clearSessionData();
    instance->setSessionName("");
    d->state = SessionState::offline;
    // NOTE: Session storage is offline. All 'write' operations will fail.
    emit sessionEnded(systemId, userId);
}

bool SessionManager::write(const QString& key, const QJsonValue& data)
{
    if (!d->hasSessionData)
        return false;

    {
        std::scoped_lock<decltype(d->guard)> lock(d->guard);
        auto& root = d->instanceData;
        if (root.contains(key))
        {
            const auto& oldData = root[key];
            if (oldData == data)
                return true;
        }
        //NX_VERBOSE(this, "write(%1) changed", key);
        root.insert(key, data);
        d->dataChanged = true;
    }

    d->saveCondition.notify_one();
    return true;
}

bool SessionManager::read(const QString& key, QJsonValue& data)
{
    NX_ASSERT(d->hasSessionData);
    if (!d->hasSessionData)
        return false;
    std::scoped_lock<decltype(d->guard)> lock(d->guard);
    const auto& root = d->instanceData;
    if (root.contains(key))
    {
        data = root[key];
        return true;
    }
    return false;
}

bool SessionManager::saveUnsafe(QJsonObject data, bool detach)
{
    if (d->instanceFileName.isEmpty())
        return false;

    // Saving data to temporary file, then copying it to final file.
    QFile outputTmpFile = {QString("%1.tmp").arg(d->instanceFileName)};
    QString instanceFileName = d->instanceFileName;

    if (!outputTmpFile.open(QIODevice::WriteOnly))
    {
        NX_ERROR(this, "save() - failed to open temporary file \"%1\"",
            outputTmpFile.fileName());
        return false;
    }

    // We should make sure there is no kSessionStateKey="open" after we detach from session.
    // It is necessary to track states from crashed clients.
    if (detach)
        data.remove(kSessionStateKey);

    QJsonDocument document{data};
    auto rawData = document.toJson();
    int size = rawData.size();

    if (outputTmpFile.write(rawData) != size)
    {
        NX_ERROR(this, "save() - failed to write data to temporary file \"%1\"", outputTmpFile.fileName());
        return false;
    }
    outputTmpFile.close();

    auto tmpFileRemover = nx::utils::makeScopeGuard(
        [&outputTmpFile]()
        {
            outputTmpFile.remove();
        });

    if (QFile::exists(instanceFileName))
        QFile::remove(instanceFileName);

    if (detach)
        d->clearSessionData();

    if (!outputTmpFile.copy(instanceFileName))
    {
        NX_ERROR(this, "save() - failed to copy data to a final file\"%1\"", outputTmpFile.fileName());
        return false;
    }

    return true;
}

/** Get a path to storage location. */
QDir SessionManager::getSessionRootDirectory() const
{
    return d->storageBasePath;
}

bool SessionManager::getSessionDirectory(const QString& sessionName, QDir& output) const
{
    QDir sessionsDataDir = getSessionRootDirectory();
    QDir sessionDir = QDir(sessionsDataDir.absoluteFilePath(sessionName));
    if (!nx::utils::file_system::ensureDir(sessionDir))
        return false;
    output = sessionDir;
    return true;
}

} // namespace nx::vms::client::desktop
