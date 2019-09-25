#include "file_watcher.h"

#include <time.h>

namespace nx::utils::file_system {

FileWatcher::FileWatcher(std::chrono::milliseconds timeout):
	m_timeout(timeout),
	m_timerPool(std::bind(&FileWatcher::checkFile, this, std::placeholders::_1)),
	m_thread(std::bind(&FileWatcher::run, this))
{
}

FileWatcher::~FileWatcher()
{
    QnMutexLocker lock(&m_mutex);
    m_terminated = true;
    lock.unlock();

	m_cond.wakeAll();
	m_thread.join();
}

SystemError::ErrorCode FileWatcher::subscribe(
	const std::string& filePath,
	Handler handler,
	SubscriptionId* const outSubscriptionId,
	FileWatcher::WatchAttributes watchAttributes)
{
	*outSubscriptionId = kInvalidSubscriptionId;

	const auto [systemError, stat] = doStat(filePath);
	if (systemError && systemError != SystemError::fileNotFound)
		return systemError;

	const bool fileExists = systemError != SystemError::fileNotFound;

	QnMutexLocker lock(&m_mutex);

	// Using std::piecewise_construct because WatchContext.subscription doesn't support move or
	// copy construction.
	auto [fileWachIter, firstInsertion] = m_fileWatches.emplace(
		std::piecewise_construct,
		std::make_tuple(filePath),
		std::make_tuple());

	if (firstInsertion)
		fileWachIter->second.fileData = FileData{fileExists, std::move(stat)};

	*outSubscriptionId = m_subscriberId++;
	m_uniqueIdToFileWatch.emplace(*outSubscriptionId, fileWachIter);
	auto& actualSubscriptionId = fileWachIter->second.subscriptionIds[*outSubscriptionId];
	fileWachIter->second.subscription.subscribe(std::move(handler), &actualSubscriptionId);
	fileWachIter->second.watchAttributes |= watchAttributes;

    m_addTimerTasks.push(std::make_tuple(filePath, m_timeout, nx::utils::monotonicTime()));

	lock.unlock();
	m_cond.wakeAll();

	return SystemError::noError;
}

void FileWatcher::unsubscribe(SubscriptionId subscriptionId)
{
	QnMutexLocker lock(&m_mutex);

	auto uniqueIdIter = m_uniqueIdToFileWatch.find(subscriptionId);
	if (uniqueIdIter == m_uniqueIdToFileWatch.end())
		return;

	auto fileWatchIter = uniqueIdIter->second;

    if (const auto it = fileWatchIter->second.subscriptionIds.find(subscriptionId);
        it != fileWatchIter->second.subscriptionIds.end())
    {
        fileWatchIter->second.subscription.removeSubscription(it->second);
        fileWatchIter->second.subscriptionIds.erase(it);
        m_uniqueIdToFileWatch.erase(uniqueIdIter);
    }

	if (fileWatchIter->second.subscriptionIds.empty())
    {
        m_timerPool.removeTimer(fileWatchIter->first);
		m_fileWatches.erase(fileWatchIter);
    }
}

void FileWatcher::run()
{
    QnMutexLocker lock(&m_mutex);
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
        const auto delay =
            timeout - duration_cast<milliseconds>(nx::utils::monotonicTime() - timeAdded);

        m_timerPool.addTimer(
            filePath,
            delay > milliseconds(0) ? delay : milliseconds(0));

        m_addTimerTasks.pop();
    }
}

void FileWatcher::checkFile(const std::string& filePath)
{
	const auto [systemError, stat] = doStat(filePath);

	QnMutexLocker lock(&m_mutex);

	auto watchIter = m_fileWatches.find(filePath);
	if (watchIter == m_fileWatches.end())
		return;

	if (systemError && systemError != SystemError::fileNotFound)
		return notify(&lock, watchIter, EventType(), systemError);

	const bool fileExists = systemError != SystemError::fileNotFound;

	if (watchIter->second.fileData.lastExists != fileExists)
	{
		watchIter->second.fileData.lastExists = fileExists;
		return notify(&lock, watchIter, fileExists ? EventType::created : EventType::deleted);
	}

	if (!metadataEqual(watchIter->second.fileData.lastStat, stat))
	{
		watchIter->second.fileData.lastStat = std::move(stat);
		return notify(&lock, watchIter, EventType::modified);
	}

    m_addTimerTasks.push(std::make_tuple(watchIter->first, m_timeout, nx::utils::monotonicTime()));
}

void FileWatcher::notify(
	MutexLocker* lock,
	FileWatches::iterator fileWatch,
	EventType eventType,
	SystemError::ErrorCode errorCode)
{
	lock->unlock();
	fileWatch->second.subscription.notify(fileWatch->first, errorCode, eventType);
	lock->relock();

    m_addTimerTasks.push(std::make_tuple(fileWatch->first, m_timeout, nx::utils::monotonicTime()));
}

std::pair<SystemError::ErrorCode, FileWatcher::Stat> FileWatcher::doStat(
	const std::string& filePath)
{
	Stat buf;
	memset(&buf, 0, sizeof(buf));

	int result = 0;
#if defined(_WIN32)
	result = _stat64(filePath.c_str(), &buf);
#elif defined(NX_UTILS_FILESYSTEM_FILEWATCHER_IOS)
	result = stat(filePath.c_str(), &buf);
#else // linux, mac
	result = stat64(filePath.c_str(), &buf);
#endif // defined(_WIN32)

	const auto systemError = result
		? SystemError::getLastOSErrorCode()
		: SystemError::noError;

	return std::make_pair(systemError, std::move(buf));
}

bool FileWatcher::metadataEqual(const Stat& a, const Stat& b)
{
	return
		a.st_size == b.st_size
		&& a.st_ctime == b.st_ctime
		&& a.st_mtime == b.st_mtime;
}

} // namespace nx::utils::file_system
