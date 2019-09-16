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
	Handler handler,
	SubscriptionId* const outSubscriptionId,
	FileWatcher::WatchAttributes watchAttributes)
{
	*outSubscriptionId = kInvalidSubscriptionId;

	QnMutexLocker lock(&m_mutex);

	const bool firstInsertion = m_fileWatches.find(filePath) == m_fileWatches.end();
	WatchContext* watchContext = firstInsertion ? nullptr : &m_fileWatches[filePath];
	bool fileExists = false;

	if (firstInsertion)
	{
		const auto [err, stat] = doStat(filePath);
		if (err && err != ENOENT)
			return false;

		fileExists = err != ENOENT;
		watchContext = &m_fileWatches[filePath];
		watchContext->fileData.lastExists = fileExists;
		watchContext->fileData.lastStat = std::move(stat);
	}

	*outSubscriptionId = m_uniqueId++;
	auto& actualSubscriptionId = watchContext->subscriptionIds[*outSubscriptionId];
	watchContext->subscription.subscribe(std::move(handler), &actualSubscriptionId);
	watchContext->watchAttributes |= watchAttributes;

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

			const auto [err, stat] = doStat(filePath);
			if (err && err != ENOENT)
				continue;
			const bool fileExists = err != ENOENT;

			lock.relock();

			if (!fileExists && watch.second.fileData.lastExists)
			{
				watch.second.fileData.lastExists = false;
				notify(&lock, &watch, EventType::deleted);
				continue;
			}

			if (fileExists && !watch.second.fileData.lastExists)
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
	}
}

void FileWatcher::notify(
	MutexLocker* lock,
	FileWatchIterator* fileWatch,
	EventType eventType)
{
	lock->unlock();
	fileWatch->second.subscription.notify(fileWatch->first, eventType);
	lock->relock();
}

std::pair <int, FileWatcher::Stat> FileWatcher::doStat(const std::string& filePath)
{
	Stat buf;
	memset(&buf, 0, sizeof(buf));

	int result;
#ifdef _WIN32
	result = _stat64(filePath.c_str(), &buf);
#else
	result = stat64(filePath.c_str(), &buf);
#endif // _WIN32

	if (result)
		result = errno;

	return std::make_pair(result, std::move(buf));
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
