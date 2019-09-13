#include "file_watcher.h"

#include <errno.h>

namespace nx::utils::file_system {

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

bool FileWatcher::subscribe(
	const std::string& filePath,
	FileModifiedHandler handler,
	SubscriptionId* const outSubscriptionId)
{
	*outSubscriptionId = kInvalidSubscriptionId;

	QnMutexLocker lock(&m_mutex);

	const bool firstInsertion = m_fileWatches.find(filePath) == m_fileWatches.end();
	WatchContext* watchContext = firstInsertion ? nullptr : &m_fileWatches[filePath];
	bool fileExists = false;

	if (firstInsertion)
	{
		Stat currentStat;
		memset(&currentStat, 0, sizeof(currentStat));

		const int result = doStat(filePath.c_str(), &currentStat);
		const int error = errno;
		if (result != 0 && error != ENOENT)
			return false;

		fileExists = result == 0 || error != ENOENT;
		watchContext = &m_fileWatches[filePath];
		watchContext->fileData.lastExists = fileExists;
		watchContext->fileData.lastStat = std::move(currentStat);
	}

	*outSubscriptionId = m_uniqueId++;

	auto& actualSubscriptionId = watchContext->subscriptionIds[*outSubscriptionId];
	watchContext->subscription.subscribe(std::move(handler), &actualSubscriptionId);

	return true;
}

void FileWatcher::unsubscribe(SubscriptionId subscriptionId)
{
	QnMutexLocker lock(&m_mutex);

	for (auto& watch : m_fileWatches)
	{
		if (watch.second.subscriptionIds.erase(subscriptionId) > 0)
		{
			watch.second.subscription.removeSubscription(subscriptionId);
			return;
		}
	}
}

std::vector<std::string> FileWatcher::paths() const
{
	QnMutexLocker lock(&m_mutex);
	std::vector<std::string> paths;
	std::transform(
		m_fileWatches.begin(),
		m_fileWatches.end(),
		std::back_inserter(paths),
		[](const auto& element) { return element.first; });
	return paths;
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

			const auto filePath = watch.first;

			lock.unlock();

			Stat currentStat;
			const int result = doStat(filePath.c_str(), &currentStat);
			const int error = errno;
			if (result != 0 && error != ENOENT)
				continue;
			const bool fileExists = result == 0 || error != ENOENT;

			lock.relock();

			if (!fileExists && watch.second.fileData.lastExists)
			{
				watch.second.fileData.lastExists = false;
				notify(&lock, &watch, WatchEvent::deleted);
				continue;
			}

			if (fileExists && !watch.second.fileData.lastExists)
			{
				watch.second.fileData.lastExists = true;
				notify(&lock, &watch, WatchEvent::created);
				continue;
			}

			if (!metadataEqual(watch.second.fileData.lastStat, currentStat))
			{
				watch.second.fileData.lastStat = std::move(currentStat);
				notify(&lock, &watch, WatchEvent::modified);
				continue;
			}
		}

		lock.unlock();
		std::this_thread::sleep_for(m_timeout);
	}
}

void FileWatcher::notify(
	MutexLocker* lock,
	FileWatchIterator* fileWatch,
	WatchEvent watchEvent)
{
	lock->unlock();
	fileWatch->second.subscription.notify(fileWatch->first, watchEvent);
	lock->relock();
}

int FileWatcher::doStat(const char* filePath, Stat* buf)
{
#ifdef _WIN32
	return _stat64(filePath, buf);
#else
	return stat64(filePath, buf);
#endif
}

bool FileWatcher::metadataEqual(const Stat& a, const Stat& b)
{
	return
		a.st_ctime == b.st_ctime
		&& a.st_mtime == b.st_mtime
		&& a.st_size == b.st_size
	    && a.st_atime == b.st_atime;
}

} // namespace nx::utils::file_system
