#pragma once

#include <thread>
#include <sys/stat.h>

#include "subscription.h"

namespace nx::utils::file_system {

/**
 * Watches files for modifications and notifies when a file has been modified.
 */
class NX_UTILS_API FileWatcher
{
public:
	enum class EventType
	{
		created,
		modified,
		deleted,
	};

	enum WatchAttributes
	{
		metadata = 1 << 0,
	};

	using Handler = MoveOnlyFunc<void(const std::string&, EventType)>;

public:
	/**
	 * @param timeout the amount of time to wait between polling files for changes
	 */
	FileWatcher(std::chrono::milliseconds timeout);
	~FileWatcher();

	/**
	 * @return true subscribed successfully, false otherwise. If subscription failed,
	 * SystemError::GetLastOsErrorCode() is set.
	 */
	 bool subscribe(
		 const std::string& filePath,
		 Handler handler,
		 nx::utils::SubscriptionId* const outSubscriptionId,
		 WatchAttributes watchAttributes = WatchAttributes::metadata);

	void unsubscribe(nx::utils::SubscriptionId subscriptionId);

	std::vector<std::string> paths() const;

private:
#ifdef _WIN32
	// MSVC complains about "using Stat = struct _stat64;"
	struct Stat: public _stat64 {};
#else
	using Stat = struct stat64;
#endif // _WIN32

	using UniqueId = SubscriptionId;

	struct FileData
	{
		bool lastExists = false;
		Stat lastStat;
	};

	struct WatchContext
	{
		FileData fileData;
		int watchAttributes = 0;
		Subscription<std::string, EventType> subscription;
		std::map<UniqueId, SubscriptionId> subscriptionIds;
	};

	using FileWatches = std::map<std::string, WatchContext>;
	using FileWatchIterator = FileWatches::iterator::value_type;

private:
	void run();

	void notify(
		nx::utils::MutexLocker* lock,
		FileWatchIterator* fileWatch,
		EventType watchEvent);

	/**
	 * @return std::pair.first 0 on success or errno on failure
	 */
	static std::pair<int, Stat> doStat(const std::string& filePath);
	static bool metadataEqual(const Stat& a, const Stat& b);

private:
	std::chrono::milliseconds m_timeout;
	std::atomic_bool m_terminated = false;
	mutable nx::utils::Mutex m_mutex;
	FileWatches m_fileWatches;
	UniqueId m_uniqueId = 0;
	std::thread m_thread;
};

} // namespace nx::utils