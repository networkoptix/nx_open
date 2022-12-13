// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <thread>
#include <sys/types.h>
#include <sys/stat.h>
#include <queue>
#include <set>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
#define NX_UTILS_FILESYSTEM_FILEWATCHER_IOS
#endif // TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
#endif // defined(__APPLE__)

#include "elapsed_timer_pool.h"
#include "subscription.h"
#include "system_error.h"

namespace nx::utils::file_system {

/**
 * Watches files for modifications and notifies when a file has been modified.
 * NOTE: On Windows, the resolution for file modification detection is one second. Files created
 * and modified within one second will not be detected as modified.
 */
class NX_UTILS_API FileWatcher
{
public:
    enum class EventType
    {
        created = 1,
        modified,
        deleted,
    };

    enum WatchAttributes
    {
        metadata = 1 << 0, //< Check file metadata for changes, e.g, creation time, modified time.
        hash = 1 << 1, //< Compute hash of file contents for changes.
        endOfEnum = 1 << 2, //< Not a WatchAttribute.
    };

    using Handler = MoveOnlyFunc<void(const std::string&, SystemError::ErrorCode, EventType)>;

public:
    /**
     * @param timeout the amount of time to wait between polling files for changes.
     */
    FileWatcher(std::chrono::milliseconds timeout);
    ~FileWatcher();

    /**
     * @return If the file does not exist at time of subscription, SystemError::fileNotFound is
     * returned. It should not be treated as an error; the user is still subscribed in this case.
     * If something went wrong while checking the file, SystemError::getLastOsErrorCode is
     * returned. Otherwise, no errors occured and SystemError::noError is returned.
     */
    SystemError::ErrorCode subscribe(
        const std::string& filePath,
        Handler handler,
        nx::utils::SubscriptionId* const outSubscriptionId,
        int watchAttributes = WatchAttributes::metadata);

    void unsubscribe(nx::utils::SubscriptionId subscriptionId);

private:
#if defined(_WIN32)
    // MSVC complains about undefined struct when "using Stat = struct _stat64;"
    struct Stat: public _stat64 {};
#elif defined(__APPLE__)
    using Stat = struct stat;
#else // linux
    using Stat = struct stat64;
#endif // defined(_WIN32)

    using UniqueId = SubscriptionId;

    struct FileData
    {
        bool lastExists = false;
        Stat lastStat;
        std::string lastHash;

        FileData() { memset(&lastStat, 0, sizeof(lastStat)); }
    };

    struct Subscriber
    {
        SubscriptionId actualSubscriptionId;
        int watchAttributes;
    };

    struct WatchContext
    {
        FileData fileData;
        int watchAttributes = 0;
        Subscription<std::string, SystemError::ErrorCode, EventType> subscription;
        std::map<UniqueId, Subscriber> uniqueIdToSubscriber;
        std::map<WatchAttributes, std::set<Subscriber*>> watchTypeToSubscriber;
    };

    using FileWatches = std::map<std::string, std::shared_ptr<WatchContext>>;

private:
    void addSubscriber(
        FileWatches::iterator fileWatch,
        Handler handler,
        nx::utils::SubscriptionId* const outSubscriptionId,
        int watchAttributes);
    void removeSubscriber(WatchContext* watchContext, UniqueId subscriberId);

    void run();
    void processAddTimerTasksUnsafe();

    void checkFile(const std::string& filePath);

    void notify(
        MutexLocker* lock,
        FileWatches::iterator watchIter,
        EventType watchEvent,
        SystemError::ErrorCode errorCode = SystemError::noError);

    static std::pair<SystemError::ErrorCode, Stat> doStat(const std::string& filePath);
    static std::pair<SystemError::ErrorCode, std::string> doHash(const std::string& filePath);
    static bool metadataEqual(const Stat& a, const Stat& b);

private:
    std::chrono::milliseconds m_timeout;
    mutable Mutex m_mutex;
    bool m_terminated = false;
    WaitCondition m_cond;
    UniqueId m_subscriberId = 0;
    ElapsedTimerPool<std::string> m_timerPool;
    FileWatches m_fileWatches;
    std::map<UniqueId, FileWatches::iterator> m_uniqueIdToFileWatch;
    std::queue<std::tuple<
        std::string /*filePath*/,
        std::chrono::milliseconds /*timeout*/,
        std::chrono::steady_clock::time_point /*timeAdded*/>>
        m_addTimerTasks;
    std::thread m_thread;
};

std::string NX_UTILS_API toString(FileWatcher::EventType eventType);

} // namespace nx::utils::file_system
