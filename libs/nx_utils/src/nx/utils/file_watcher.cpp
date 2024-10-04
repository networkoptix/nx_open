// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "file_watcher.h"

#include <fstream>
#include <time.h>

#include "cryptographic_hash.h"
#include "log/log_main.h"

namespace nx::utils::file_system {

FileWatcher::FileWatcher(std::chrono::milliseconds timeout):
    m_timeout(timeout),
    m_timerPool(std::bind(&FileWatcher::checkFile, this, std::placeholders::_1)),
    m_thread(std::bind(&FileWatcher::run, this))
{
}

FileWatcher::~FileWatcher()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_terminated = true;
    lock.unlock();

    m_cond.wakeAll();
    m_thread.join();
}

SystemError::ErrorCode FileWatcher::subscribe(
    const std::string& filePath,
    Handler handler,
    SubscriptionId* const outSubscriptionId,
    int watchAttributes)
{
    NX_ASSERT(watchAttributes > 0 && watchAttributes < WatchAttributes::endOfEnum);

    *outSubscriptionId = kInvalidSubscriptionId;

    NX_MUTEX_LOCKER lock(&m_mutex);

    if (auto it = m_fileWatches.find(filePath); it != m_fileWatches.end())
    {
        if ((it->second->watchAttributes & watchAttributes) == watchAttributes)
        {
            addSubscriber(it, std::move(handler), outSubscriptionId, watchAttributes);
            return it->second->fileData.lastExists
                ? SystemError::noError
                : SystemError::fileNotFound;
        }
    }

    lock.unlock();

    const bool needStat = watchAttributes & WatchAttributes::metadata;
    const bool needHash = watchAttributes & WatchAttributes::hash;

    SystemError::ErrorCode statError = SystemError::noError;
    Stat stat{};
    if (needStat)
    {
        std::tie(statError, stat) = doStat(filePath);
        if (statError && statError != SystemError::fileNotFound)
            return statError;
    }

    SystemError::ErrorCode hashError = SystemError::noError;
    std::string hash;
    if (needHash)
    {
        std::tie(hashError, hash) = doHash(filePath);
        if (hashError && hashError != SystemError::fileNotFound)
            return hashError;
    }

    // Both statError and hashError are either noError or fileNotFound. If they do not agree, then
    // redo both until they agree.
    if (needStat && needHash && statError != hashError)
        return subscribe(filePath, std::move(handler), outSubscriptionId, watchAttributes);

    lock.relock();

    auto [watchIter, added] = m_fileWatches.emplace(filePath, nullptr);
    if (added)
        watchIter->second = std::make_shared<WatchContext>();

    if (needStat && !(watchIter->second->watchAttributes & WatchAttributes::metadata))
    {
        watchIter->second->fileData.lastStat = std::move(stat);
        watchIter->second->fileData.lastExists = statError != SystemError::fileNotFound;
    }

    if (needHash && !(watchIter->second->watchAttributes & WatchAttributes::hash))
    {
        watchIter->second->fileData.lastHash = std::move(hash);
        watchIter->second->fileData.lastExists = hashError != SystemError::fileNotFound;
    }

    addSubscriber(watchIter, std::move(handler), outSubscriptionId, watchAttributes);

    m_addTimerTasks.emplace(filePath, m_timeout, monotonicTime());

    return needHash ? hashError : statError;
}

void FileWatcher::unsubscribe(SubscriptionId subscriptionId)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    auto uniqueIdIter = m_uniqueIdToFileWatch.find(subscriptionId);
    if (uniqueIdIter == m_uniqueIdToFileWatch.end())
        return;

    auto fileWatchIter = uniqueIdIter->second;

    removeSubscriber(fileWatchIter->second.get(), subscriptionId);

    m_uniqueIdToFileWatch.erase(uniqueIdIter);

    if (fileWatchIter->second->uniqueIdToSubscriber.empty())
    {
        m_timerPool.removeTimer(fileWatchIter->first);
        m_fileWatches.erase(fileWatchIter);
    }
}

void FileWatcher::addSubscriber(
    FileWatches::iterator watchIter,
    Handler handler,
    nx::utils::SubscriptionId* const outSubscriptionId,
    int watchAttributes)
{
    *outSubscriptionId = m_subscriberId++;

    m_uniqueIdToFileWatch.emplace(*outSubscriptionId, watchIter);

    auto& subscriber = watchIter->second->uniqueIdToSubscriber[*outSubscriptionId];
    subscriber.watchAttributes = watchAttributes;

    watchIter->second->subscription.subscribe(
        std::move(handler), &subscriber.actualSubscriptionId);

    for (int i = 1; i < WatchAttributes::endOfEnum; i <<= 1)
    {
        if (i & watchAttributes)
            watchIter->second->watchTypeToSubscriber[WatchAttributes(i)].emplace(&subscriber);
    }

    watchIter->second->watchAttributes |= watchAttributes;
}

void FileWatcher::removeSubscriber(WatchContext* watchContext, UniqueId subscriptionId)
{
    const auto it = watchContext->uniqueIdToSubscriber.find(subscriptionId);
    if (it == watchContext->uniqueIdToSubscriber.end())
        return;

    for (auto subsIter = watchContext->watchTypeToSubscriber.begin();
         subsIter != watchContext->watchTypeToSubscriber.end();)
    {
        subsIter->second.erase(&it->second);
        if (subsIter->second.empty())
        {
            // Clear this watch attribute, as there are no subscribers for it.
            watchContext->watchAttributes &= ~(1U << subsIter->first);
            subsIter = watchContext->watchTypeToSubscriber.erase(subsIter);
        }
        else
        {
            ++subsIter;
        }
    }

    if (!(watchContext->watchAttributes & WatchAttributes::metadata))
        memset(&watchContext->fileData.lastStat, 0, sizeof(Stat));

    if (!(watchContext->watchAttributes & WatchAttributes::hash))
        watchContext->fileData.lastHash.clear();

    watchContext->subscription.removeSubscription(it->second.actualSubscriptionId);
    watchContext->uniqueIdToSubscriber.erase(it);
}

void FileWatcher::run()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    while (!m_terminated)
    {
        processAddTimerTasksUnsafe();

        lock.unlock();
        m_timerPool.processTimers();
        lock.relock();

        const auto delayToNextProcessing = m_timerPool.delayToNextProcessing();

        const auto delay =
            delayToNextProcessing && *delayToNextProcessing > std::chrono::milliseconds(0)
            ? *delayToNextProcessing
            : m_timeout;

        if (!m_terminated)
            m_cond.wait(lock.mutex(), delay);
    }
}

void FileWatcher::processAddTimerTasksUnsafe()
{
    using namespace std::chrono;

    while (!m_addTimerTasks.empty())
    {
        const auto& [filePath, timeout, timeAdded] = m_addTimerTasks.front();
        const auto delay = timeout - duration_cast<milliseconds>(monotonicTime() - timeAdded);

        m_timerPool.addTimer(filePath, delay > milliseconds(0) ? delay : milliseconds(0));

        m_addTimerTasks.pop();
    }
}

void FileWatcher::checkFile(const std::string& filePath)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    auto watchIter = m_fileWatches.find(filePath);
    if (watchIter == m_fileWatches.end())
        return;

    bool needStat = watchIter->second->watchAttributes & WatchAttributes::metadata;
    bool needHash = watchIter->second->watchAttributes & WatchAttributes::hash;

    lock.unlock();

    SystemError::ErrorCode statError = SystemError::noError;
    Stat stat{};
    if (needStat)
        std::tie(statError, stat) = doStat(filePath);

    SystemError::ErrorCode hashError = SystemError::noError;
    std::string hash;
    if (needHash)
        std::tie(hashError, hash) = doHash(filePath);

    lock.relock();

    watchIter = m_fileWatches.find(filePath);
    if (watchIter == m_fileWatches.end())
        return;

    if (needStat && statError && statError != SystemError::fileNotFound)
        return notify(&lock, watchIter, EventType(), statError);

    if (needHash && hashError && hashError != SystemError::fileNotFound)
        return notify(&lock, watchIter, EventType(), hashError);

    // Both statError and hashError are either noError or fileNotFound. If they do not agree, then
    // redo both until they agree.
    if (needStat && needHash && statError != hashError)
    {
        m_addTimerTasks.emplace(filePath, std::chrono::milliseconds(0), monotonicTime());
        return;
    }

    {
        const bool newNeedStat = watchIter->second->watchAttributes & WatchAttributes::metadata;
        const bool newNeedHash = watchIter->second->watchAttributes & WatchAttributes::hash;
        // Either stat or hash was added or dropped while mutex was unlocked, during lengthy stat
        // or hash operation. Unlikely, but better to handle it.
        if (needStat != newNeedStat || needHash != newNeedHash)
        {
            NX_DEBUG(this, "watch attributes changed within checkFile()");
            m_addTimerTasks.emplace(filePath, std::chrono::milliseconds(0), monotonicTime());
            return;
        }
    }

    std::optional<EventType> notifyEvent;

    if (needStat)
    {
        const bool fileExists = statError != SystemError::fileNotFound;
        if (watchIter->second->fileData.lastExists != fileExists)
            notifyEvent = fileExists ? EventType::created : EventType::deleted;
        else if (!metadataEqual(watchIter->second->fileData.lastStat, stat))
            notifyEvent = EventType::modified;
    }

    if (needHash && !notifyEvent)
    {
        const bool fileExists = hashError != SystemError::fileNotFound;
        if (watchIter->second->fileData.lastExists != fileExists)
            notifyEvent = fileExists ? EventType::created : EventType::deleted;
        else if (watchIter->second->fileData.lastHash != hash)
            notifyEvent = EventType::modified;
    }

    if (notifyEvent)
    {
        if (notifyEvent == EventType::deleted)
        {
            watchIter->second->fileData = FileData();
        }
        else //< created or modified. Either way, need to update file data.
        {
            watchIter->second->fileData.lastExists = true;
            if (needStat)
                watchIter->second->fileData.lastStat = std::move(stat);
            if (needHash)
                watchIter->second->fileData.lastHash = std::move(hash);
        }

        return notify(&lock, watchIter, *notifyEvent);
    }

    m_addTimerTasks.emplace(watchIter->first, m_timeout, monotonicTime());
}

void FileWatcher::notify(
    MutexLocker* lock,
    FileWatches::iterator watchIter,
    EventType eventType,
    SystemError::ErrorCode errorCode)
{
    auto filePath = watchIter->first;
    auto watchContext = watchIter->second;

    lock->unlock();
    watchContext->subscription.notify(filePath, errorCode, eventType);
    lock->relock();

    m_addTimerTasks.emplace(std::move(filePath), m_timeout, monotonicTime());
}

std::pair<SystemError::ErrorCode, FileWatcher::Stat> FileWatcher::doStat(
    const std::string& filePath)
{
    Stat buf;
    memset(&buf, 0, sizeof(buf));

    int result = 0;
#if defined(_WIN32)
    result = _stat64(filePath.c_str(), &buf);
#elif defined(__APPLE__)
    result = stat(filePath.c_str(), &buf);
#else
    result = stat64(filePath.c_str(), &buf);
#endif // defined(_WIN32)

    const auto systemError = result ? SystemError::getLastOSErrorCode() : SystemError::noError;

    return std::make_pair(systemError, std::move(buf));
}

std::pair<SystemError::ErrorCode, std::string> FileWatcher::doHash(const std::string& filePath)
{
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open())
        return std::make_pair(SystemError::getLastOSErrorCode(), std::string());

    QnCryptographicHash hash(QnCryptographicHash::Md5);

    std::array<char, 1024> buf;
    while (!file.eof())
    {
        if (file.read(buf.data(), buf.size()).fail() && !file.eof())
            return std::make_pair(SystemError::getLastOSErrorCode(), std::string());

        hash.addData(buf.data(), (int) file.gcount());
    }

    return std::make_pair(SystemError::noError, hash.result().toStdString());
}

bool FileWatcher::metadataEqual(const Stat& a, const Stat& b)
{
#if defined(_WIN32)
    return a.st_size == b.st_size && a.st_mtime == b.st_mtime && a.st_ctime == b.st_ctime;
#else
    const auto timespecEqual =
        [](const timespec& a, const timespec& b)
        {
            return a.tv_sec == b.tv_sec && a.tv_nsec == b.tv_nsec;
        };

#if defined(__APPLE__)
    return a.st_size == b.st_size
        && timespecEqual(a.st_mtimespec, b.st_mtimespec)
        && timespecEqual(a.st_ctimespec, b.st_ctimespec);
#else
    return a.st_size == b.st_size
        && timespecEqual(a.st_mtim, b.st_mtim)
        && timespecEqual(a.st_ctim, b.st_ctim);
#endif // __APPLE__
#endif // _WIN32
}

std::string toString(FileWatcher::EventType eventType)
{
    switch (eventType)
    {
        case FileWatcher::EventType::created:
            return "created";
        case FileWatcher::EventType::modified:
            return "modified";
        case FileWatcher::EventType::deleted:
            return "deleted";
    }
    return "unknown";
}

} // namespace nx::utils::file_system
