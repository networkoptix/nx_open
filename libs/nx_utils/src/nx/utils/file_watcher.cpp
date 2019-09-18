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
	m_terminated = true;
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
	auto [it, firstInsertion] = m_fileWatches.emplace(
		std::piecewise_construct,
		std::make_tuple(filePath),
		std::make_tuple());

	if (firstInsertion)
		it->second.fileData = FileData{fileExists, std::move(stat)};

	*outSubscriptionId = m_subscriberId++;
	m_uniqueIdToFileWatch.emplace(*outSubscriptionId, it);
	auto& actualSubscriptionId = it->second.subscriptionIds[*outSubscriptionId];
	it->second.subscription.subscribe(std::move(handler), &actualSubscriptionId);
	it->second.watchAttributes |= watchAttributes;

	m_timerPool.addTimer(filePath, m_timeout);

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

	if (fileWatchIter->second.subscriptionIds.erase(subscriptionId) > 0)
	{
		fileWatchIter->second.subscription.removeSubscription(subscriptionId);
		m_uniqueIdToFileWatch.erase(uniqueIdIter);
		m_timerPool.removeTimer(fileWatchIter->first);
	}

	if (fileWatchIter->second.subscriptionIds.empty())
		m_fileWatches.erase(fileWatchIter);
}

void FileWatcher::run()
{
	while (!m_terminated)
	{
		m_timerPool.processTimers();

		QnMutexLocker lock(&m_mutex);

		const auto delayToNextProcessing = m_timerPool.delayToNextProcessing();

		// No jobs, so sleep and try again later.
		if (!delayToNextProcessing)
		{
			m_cond.wait(lock.mutex(), m_timeout);
			continue;
		}

		if (*delayToNextProcessing > std::chrono::milliseconds(0))
			m_cond.wait(lock.mutex(), *delayToNextProcessing);
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

	m_timerPool.addTimer(filePath, m_timeout);
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

	m_timerPool.addTimer(fileWatch->first, m_timeout);
}

std::pair<SystemError::ErrorCode, FileWatcher::Stat> FileWatcher::doStat(
	const std::string& filePath)
{
	Stat buf;
	memset(&buf, 0, sizeof(buf));

	int result = 0;
#ifdef _WIN32
	result = _stat64(filePath.c_str(), &buf);
#else
	result = stat64(filePath.c_str(), &buf);
#endif // _WIN32

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
		&& a.st_mtime == b.st_mtime
	    && a.st_atime == b.st_atime;
}

} // namespace nx::utils::file_system
