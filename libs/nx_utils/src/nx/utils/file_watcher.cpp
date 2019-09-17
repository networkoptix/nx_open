#include "file_watcher.h"

#include <errno.h>

namespace nx::utils::file_system {

FileWatcher::WatchContext::WatchContext()
{
	subscription =
		std::make_unique<Subscription<std::string, SystemError::ErrorCode, EventType>>();
}

FileWatcher::FileWatcher(std::chrono::milliseconds timeout):
	m_timeout(timeout),
	m_thread(std::bind(&FileWatcher::run, this))
{
}

FileWatcher::~FileWatcher()
{
	m_terminated = true;
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

	auto [it, firstInsertion] = m_fileWatches.emplace(filePath, WatchContext());
	if (firstInsertion)
		it->second.fileData = FileData{fileExists, std::move(stat)};

	*outSubscriptionId = m_uniqueId++;
	auto& actualSubscriptionId = it->second.subscriptionIds[*outSubscriptionId];
	it->second.subscription->subscribe(std::move(handler), &actualSubscriptionId);
	it->second.watchAttributes |= watchAttributes;

	return SystemError::noError;
}

void FileWatcher::unsubscribe(SubscriptionId subscriptionId)
{
	QnMutexLocker lock(&m_mutex);

	for (auto& watch : m_fileWatches)
	{
		if (watch.second.subscriptionIds.erase(subscriptionId) > 0)
		{
			watch.second.subscription->removeSubscription(subscriptionId);
			return;
		}
	}
}

void FileWatcher::run()
{
	while (!m_terminated)
	{
		QnMutexLocker lock(&m_mutex);
		for(auto& watch: m_fileWatches)
		{
			if (watch.second.subscriptionIds.empty())
				continue;

			lock.unlock();
			const auto [systemError, stat] = doStat(watch.first);
			lock.relock();

			if (systemError && systemError != SystemError::fileNotFound)
			{
				notify(&lock, &watch, EventType(), systemError);
				continue;
			}

			const bool fileExists = systemError != SystemError::fileNotFound;

			if (watch.second.fileData.lastExists && !fileExists)
			{
				watch.second.fileData.lastExists = false;
				notify(&lock, &watch, EventType::deleted);
				continue;
			}

			if (!watch.second.fileData.lastExists && fileExists)
			{
				watch.second.fileData.lastExists = true;
				notify(&lock, &watch, EventType::created);
				continue;
			}

			if (!metadataEqual(watch.second.fileData.lastStat, stat))
			{
				watch.second.fileData.lastStat = std::move(stat);
				notify(&lock, &watch, EventType::modified);
				continue;
			}
		}

		lock.unlock();
		std::this_thread::sleep_for(m_timeout);
		lock.relock();
	}
}

void FileWatcher::notify(
	MutexLocker* lock,
	FileWatchIterator* fileWatch,
	EventType eventType,
	SystemError::ErrorCode errorCode)
{
	lock->unlock();
	fileWatch->second.subscription->notify(fileWatch->first, errorCode, eventType);
	lock->relock();
}

std::pair<SystemError::ErrorCode, FileWatcher::Stat> FileWatcher::doStat(
	const std::string& filePath)
{
	Stat buf;
	memset(&buf, 0, sizeof(buf));

	int result;
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
