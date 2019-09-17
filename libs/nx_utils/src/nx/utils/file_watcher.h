#pragma once

#include <thread>
#include <sys/stat.h>

#include "elapsed_timer_pool.h"
#include "subscription.h"
#include "system_error.h"

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
		metadata = 1 << 0, //< Check file metadata for changes, e.g, creation time, modified time
	};

	using Handler =
		MoveOnlyFunc<void(const std::string&, SystemError::ErrorCode, EventType)>;

public:
	/**
	 * @param timeout the amount of time to wait between polling files for changes.
	 */
	FileWatcher(std::chrono::milliseconds timeout);
	~FileWatcher();

	/**
	 * @return SystemError::noError if subscribed successfully, some error code otherwise.
	 */
	 SystemError::ErrorCode subscribe(
		 const std::string& filePath,
		 Handler handler,
		 nx::utils::SubscriptionId* const outSubscriptionId,
		 WatchAttributes watchAttributes = WatchAttributes::metadata);

	void unsubscribe(nx::utils::SubscriptionId subscriptionId);

private:
#ifdef _WIN32
	// MSVC complains about undefined struct when "using Stat = struct _stat64;"
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
		// unique_ptr because Subscription doesn't support copy construction
		std::unique_ptr<Subscription<std::string, SystemError::ErrorCode, EventType>> subscription;
		std::map<UniqueId, SubscriptionId> subscriptionIds;

		WatchContext();
	};

	using FileWatches = std::map<std::string, WatchContext>;

private:
	void run();

	void checkFile(const std::string& filePath);

	void notify(
		nx::utils::MutexLocker* lock,
		FileWatches::iterator fileWatch,
		EventType watchEvent,
		SystemError::ErrorCode errorCode = SystemError::noError);

	static std::pair<SystemError::ErrorCode, Stat> doStat(const std::string& filePath);
	static bool metadataEqual(const Stat& a, const Stat& b);

private:
	std::chrono::milliseconds m_timeout;
	std::atomic_bool m_terminated = false;
	mutable Mutex m_mutex;
	WaitCondition m_cond;
	UniqueId m_subscriberId = 0;
	ElapsedTimerPool<std::string> m_timerPool;
	FileWatches m_fileWatches;
	std::map<UniqueId, FileWatches::iterator> m_uniqueIdToFileWatch;
	std::thread m_thread;
};

} // namespace nx::utils