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
		m_timeout = std::chrono::milliseconds(10);
		m_fileWatcher = std::make_unique<file_system::FileWatcher>(m_timeout);
		m_filePath = testDataDir().toStdString() + "/test.txt";
	}

	virtual void TearDown() override
	{
		m_fileWatcher.reset();
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
		std::this_thread::sleep_for(std::chrono::seconds(2));
		std::ofstream file(m_filePath, std::ios::out | std::ios::trunc);
		file << "data";
		file.close();
	}

	void whenFileIsDeleted()
	{
		ASSERT_EQ(0, remove(m_filePath.c_str()));
	}

	void whenRemoveSubscription()
	{
		m_fileWatcher->unsubscribe(m_subscriptionIds.back());

		// Removing any existing events to check for "no events emitted" later
		while (!m_fileEvents.empty())
			m_fileEvents.pop();
	}

	void whenFileisRead()
	{
		std::this_thread::sleep_for(std::chrono::seconds(1));

		std::ifstream file(m_filePath);
		std::string contents{
			std::istreambuf_iterator<char>(file),
			std::istreambuf_iterator<char>()};
		file.close();
	}

	void thenSubscriberIsNotified(file_system::FileWatcher::EventType eventType)
	{
		m_lastEvent = m_fileEvents.pop();
		ASSERT_EQ(m_filePath, std::get<0>(m_lastEvent));
		ASSERT_EQ(eventType, std::get<1>(m_lastEvent));
	}

	void thenAllSubscribersAreNotified(file_system::FileWatcher::EventType eventType)
	{
		for (int i = 0; i < (int)m_subscriptionIds.size(); ++i)
			thenSubscriberIsNotified(eventType);
	}

	void thenSubscriberIsNotNotified()
	{
		thenFormerSubscriberIsNotNotified();
	}

	void thenFormerSubscriberIsNotNotified()
	{
		std::this_thread::sleep_for(std::chrono::seconds(1));
		ASSERT_TRUE(m_fileEvents.empty());
	}

private:
	void createFile(const std::string& filePath)
	{
		std::ofstream file(filePath);
		file << "data";
		file.close();
	}

	void subscribe(const std::string& filePath)
	{
		m_subscriptionIds.emplace_back(kInvalidSubscriptionId);
		m_fileWatcher->subscribe(
			filePath,
			[this](const auto& filePath, auto systemErrorCode, const auto eventType)
			{
				ASSERT_EQ(SystemError::noError, systemErrorCode);
				m_fileEvents.push(std::make_tuple(filePath, eventType));
			},
			&m_subscriptionIds.back());
	}

private:
	std::chrono::milliseconds m_timeout;
	std::unique_ptr<file_system::FileWatcher> m_fileWatcher;
	SyncQueue<std::tuple<std::string, file_system::FileWatcher::EventType>> m_fileEvents;
	std::string m_filePath;
	std::tuple<std::string, file_system::FileWatcher::EventType> m_lastEvent;
	std::vector<SubscriptionId> m_subscriptionIds;
};

TEST_F(FileWatcher, file_created)
{
	givenNonExistentFile();
	givenSubscription();

	whenFileIsCreated();

	thenSubscriberIsNotified(file_system::FileWatcher::EventType::created);
}

TEST_F(FileWatcher, file_modified)
{
	givenExistingFile();
	givenSubscription();

	whenFileIsModified();

	thenSubscriberIsNotified(file_system::FileWatcher::EventType::modified);
}

TEST_F(FileWatcher, file_deleted)
{
	givenExistingFile();
	givenSubscription();

	whenFileIsDeleted();

	thenSubscriberIsNotified(file_system::FileWatcher::EventType::deleted);
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

	thenAllSubscribersAreNotified(file_system::FileWatcher::EventType::created);
}

TEST_F(FileWatcher, file_read_does_not_trigger_file_modified_event)
{
	givenExistingFile();
	givenSubscription();

	whenFileisRead();

	thenSubscriberIsNotNotified();
}

} // namespace nx::utils::test