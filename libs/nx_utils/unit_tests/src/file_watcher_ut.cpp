#include <gtest/gtest.h>
#include <nx/utils/test_support/test_with_temporary_directory.h>

#include <stdio.h>

#include <nx/utils/file_watcher.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx::utils::test {

class FileWatcher:
	public testing::Test,
	public TestWithTemporaryDirectory
{
public:
	FileWatcher(): TestWithTemporaryDirectory("file_watcher_ut") {}

protected:
	virtual void SetUp() override
	{
		m_fileWatcher = std::make_unique<file_system::FileWatcher>(std::chrono::milliseconds(10));
		m_filePath = testDataDir().toStdString() + "/test.txt";
	}

	virtual void TearDown() override
	{
		for(const auto& subscriptionId: m_subscriptionIds)
			m_fileWatcher->unsubscribe(subscriptionId);
	}

	void givenNonExistentFile()
	{
		// Do nothing, file already doesn't exist.
	}

	void givenExistingFile()
	{
		createFile(m_filePath);
	}

	void givenSubscription()
	{
		subscribe(m_filePath);
	}

	void givenMultipleSubscriptions()
	{
		for (int i = 0; i < 2; ++i)
			subscribe(m_filePath);
	}

	void whenFileIsCreated()
	{
		createFile(m_filePath);
	}

	void whenFileIsModified()
	{
		std::ofstream file(m_filePath);
		file << "modified";
		file.close();
	}

	void whenFileIsDeleted()
	{
		ASSERT_EQ(0, remove(m_filePath.c_str()));
	}

	void whenRemoveSubscription()
	{
		m_fileWatcher->unsubscribe(m_subscriptionIds.back());

		while(!m_watchEvents.empty())
			m_watchEvents.pop();
	}

	void thenSubscriberIsNotified(file_system::WatchEvent watchEvent)
	{
		m_lastEvent = m_watchEvents.pop();
		ASSERT_EQ(m_filePath, std::get<0>(m_lastEvent));
		ASSERT_EQ(watchEvent, std::get<1>(m_lastEvent));
	}

	void thenAllSubscribersAreNotified(file_system::WatchEvent watchEvent)
	{
		for (int i = 0; i < (int) m_subscriptionIds.size(); ++i)
			thenSubscriberIsNotified(watchEvent);
	}

	void thenFormerSubscriberIsNotNotified()
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		ASSERT_TRUE(m_watchEvents.empty());
	}

private:
	void createFile(const std::string& filePath)
	{
		std::ofstream file(filePath);
	}

	void subscribe(const std::string& filePath)
	{
		m_subscriptionIds.emplace_back(kInvalidSubscriptionId);
		m_fileWatcher->subscribe(
			filePath,
			[this](const auto& filePath, const auto watchEvent)
			{
				m_watchEvents.push(std::make_tuple(filePath, watchEvent));
			},
			&m_subscriptionIds.back());
	}

private:
	std::unique_ptr<file_system::FileWatcher> m_fileWatcher;
	SyncQueue<std::tuple<std::string, file_system::WatchEvent>> m_watchEvents;
	std::string m_filePath;
	std::tuple<std::string, file_system::WatchEvent> m_lastEvent;
	std::vector<SubscriptionId> m_subscriptionIds;
};

TEST_F(FileWatcher, file_created)
{
	givenNonExistentFile();
	givenSubscription();

	whenFileIsCreated();

	thenSubscriberIsNotified(file_system::WatchEvent::created);
}

TEST_F(FileWatcher, file_modified)
{
	givenExistingFile();
	givenSubscription();

	whenFileIsModified();

	thenSubscriberIsNotified(file_system::WatchEvent::modified);
}

TEST_F(FileWatcher, file_deleted)
{
	givenExistingFile();
	givenSubscription();

	whenFileIsDeleted();

	thenSubscriberIsNotified(file_system::WatchEvent::deleted);
}

TEST_F(FileWatcher, unsubscribe)
{
	givenExistingFile();
	givenSubscription();

	whenRemoveSubscription();
	whenFileIsDeleted();

	thenFormerSubscriberIsNotNotified();
}

TEST_F(FileWatcher, multiple_subscribers)
{
	givenNonExistentFile();
	givenMultipleSubscriptions();

	whenFileIsCreated();

	thenAllSubscribersAreNotified(file_system::WatchEvent::created);
}

} // namespace nx::utils::test