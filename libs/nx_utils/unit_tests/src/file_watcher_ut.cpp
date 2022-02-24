// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>
#include <nx/utils/test_support/test_with_temporary_directory.h>

#include <stdio.h>
#include <fstream>

#include <nx/utils/file_watcher.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx::utils::file_system::test {

class FileWatcher: public testing::Test, public nx::utils::test::TestWithTemporaryDirectory
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

    void givenExistingFile(std::string_view contents = "data")
    {
        createFile(m_filePath, contents);
    }

    void givenSubscription(int watchAttributes)
    {
        subscribe(m_filePath, watchAttributes);
    }

    void givenMultipleSubscriptions(int watchAttributes)
    {
        for (int i = 0; i < 2; ++i)
            subscribe(m_filePath, watchAttributes);
    }

    void whenFileIsCreated(std::string_view contents = "data")
    {
        createFile(m_filePath, contents);
    }

    void whenFileIsModified(std::string_view contents = "data")
    {
        // windows has file modification time resolution of 2 seconds... linux is less but still
        // sleep is needed.
        if (!(m_watchAttributes & file_system::FileWatcher::WatchAttributes::hash))
            std::this_thread::sleep_for(std::chrono::seconds(2));
        NX_DEBUG(this, "modifying file");
        std::ofstream file(m_filePath, std::ios::trunc);
        file << contents;
    }

    void whenFileIsDeleted()
    {
        ASSERT_EQ(0, ::remove(m_filePath.c_str()));
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
            std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
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
        for (int i = 0; i < (int) m_subscriptionIds.size(); ++i)
            thenSubscriberIsNotified(eventType);
    }

    void thenSubscriberIsNotNotified() { thenFormerSubscriberIsNotNotified(); }

    void thenFormerSubscriberIsNotNotified()
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        ASSERT_TRUE(m_fileEvents.empty());
    }

private:
    void createFile(const std::string& filePath, std::string_view contents)
    {
        std::ofstream file(filePath);
        file << contents;
        file.close();
    }

    void subscribe(const std::string& filePath, int watchAttributes)
    {
        m_watchAttributes |= watchAttributes;
        m_subscriptionIds.emplace_back(kInvalidSubscriptionId);
        m_fileWatcher->subscribe(
            filePath,
            [this](const auto& filePath, auto systemErrorCode, const auto eventType)
            {
                ASSERT_EQ(SystemError::noError, systemErrorCode);
                NX_DEBUG(this, "got event type: %1", eventType);
                m_fileEvents.push(std::make_tuple(filePath, eventType));
            },
            &m_subscriptionIds.back(),
            watchAttributes);
    }

private:
	std::chrono::milliseconds m_timeout;
	std::unique_ptr<file_system::FileWatcher> m_fileWatcher;
	SyncQueue<std::tuple<std::string, file_system::FileWatcher::EventType>> m_fileEvents;
    int m_watchAttributes = 0;
	std::string m_filePath;
	std::tuple<std::string, file_system::FileWatcher::EventType> m_lastEvent;
	std::vector<SubscriptionId> m_subscriptionIds;
};

TEST_F(FileWatcher, file_created_metadata)
{
    givenNonExistentFile();
    givenSubscription(file_system::FileWatcher::metadata);

    whenFileIsCreated();

    thenSubscriberIsNotified(file_system::FileWatcher::EventType::created);
}

TEST_F(FileWatcher, file_created_hash)
{
    givenNonExistentFile();
    givenSubscription(file_system::FileWatcher::hash);

    whenFileIsCreated();

    thenSubscriberIsNotified(file_system::FileWatcher::EventType::created);
}

TEST_F(FileWatcher, file_created_all)
{
    givenNonExistentFile();
    givenSubscription(file_system::FileWatcher::hash | file_system::FileWatcher::metadata);

    whenFileIsCreated();

    thenSubscriberIsNotified(file_system::FileWatcher::EventType::created);
}

TEST_F(FileWatcher, file_modified_metadata)
{
    givenExistingFile();
    givenSubscription(file_system::FileWatcher::metadata);

    whenFileIsModified();

    thenSubscriberIsNotified(file_system::FileWatcher::EventType::modified);
}

TEST_F(FileWatcher, file_modified_hash)
{
    givenExistingFile("data");
    givenSubscription(file_system::FileWatcher::hash);

    whenFileIsModified("more data");

    thenSubscriberIsNotified(file_system::FileWatcher::EventType::modified);
}

TEST_F(FileWatcher, file_modified_all)
{
    givenExistingFile("data");
    givenSubscription(file_system::FileWatcher::metadata | file_system::FileWatcher::hash);

    whenFileIsModified("more data");

    thenSubscriberIsNotified(file_system::FileWatcher::EventType::modified);
}

TEST_F(FileWatcher, file_deleted_metadata)
{
    givenExistingFile();
    givenSubscription(file_system::FileWatcher::metadata);

    whenFileIsDeleted();

    thenSubscriberIsNotified(file_system::FileWatcher::EventType::deleted);
}

TEST_F(FileWatcher, file_deleted_hash)
{
    givenExistingFile();
    givenSubscription(file_system::FileWatcher::hash);

    whenFileIsDeleted();

    thenSubscriberIsNotified(file_system::FileWatcher::EventType::deleted);
}

TEST_F(FileWatcher, file_deleted_all)
{
    givenExistingFile();
    givenSubscription(file_system::FileWatcher::metadata | file_system::FileWatcher::hash);

    whenFileIsDeleted();

    thenSubscriberIsNotified(file_system::FileWatcher::EventType::deleted);
}

TEST_F(FileWatcher, unsubscribe_metadata)
{
    givenExistingFile();
    givenSubscription(file_system::FileWatcher::metadata);

    whenRemoveSubscription();
    whenFileIsDeleted();

    thenFormerSubscriberIsNotNotified();
}

TEST_F(FileWatcher, unsubscribe_hash)
{
    givenExistingFile();
    givenSubscription(file_system::FileWatcher::hash);

    whenRemoveSubscription();
    whenFileIsDeleted();

    thenFormerSubscriberIsNotNotified();
}

TEST_F(FileWatcher, unsubscribe_all)
{
    givenExistingFile();
    givenSubscription(file_system::FileWatcher::metadata | file_system::FileWatcher::hash);

    whenRemoveSubscription();
    whenFileIsDeleted();

    thenFormerSubscriberIsNotNotified();
}

TEST_F(FileWatcher, multiple_subscribers_metadata)
{
    givenNonExistentFile();
    givenMultipleSubscriptions(file_system::FileWatcher::metadata);

    whenFileIsCreated();

    thenAllSubscribersAreNotified(file_system::FileWatcher::EventType::created);
}

TEST_F(FileWatcher, multiple_subscribers_hash)
{
    givenNonExistentFile();
    givenMultipleSubscriptions(file_system::FileWatcher::metadata);

    whenFileIsCreated();

    thenAllSubscribersAreNotified(file_system::FileWatcher::EventType::created);
}

TEST_F(FileWatcher, multiple_subscribers_different_attributes)
{
    givenNonExistentFile();
    givenMultipleSubscriptions(file_system::FileWatcher::metadata);
    givenMultipleSubscriptions(file_system::FileWatcher::hash);

    whenFileIsCreated();

    thenAllSubscribersAreNotified(file_system::FileWatcher::EventType::created);
}

TEST_F(FileWatcher, multiple_subscribers_all)
{
    givenNonExistentFile();
    givenMultipleSubscriptions(
        file_system::FileWatcher::metadata | file_system::FileWatcher::hash);

    whenFileIsCreated();

    thenAllSubscribersAreNotified(file_system::FileWatcher::EventType::created);
}

TEST_F(FileWatcher, file_read_does_not_trigger_file_modified_event_metadata)
{
	givenExistingFile();
    givenSubscription(file_system::FileWatcher::metadata);

    whenFileisRead();

    thenSubscriberIsNotNotified();
}

TEST_F(FileWatcher, file_read_does_not_trigger_file_modified_event_hash)
{
    givenExistingFile();
    givenSubscription(file_system::FileWatcher::hash);

    whenFileisRead();

    thenSubscriberIsNotNotified();
}

TEST_F(FileWatcher, file_read_does_not_trigger_file_modified_event_all)
{
    givenExistingFile();
    givenSubscription(file_system::FileWatcher::metadata | file_system::FileWatcher::hash);

    whenFileisRead();

    thenSubscriberIsNotNotified();
}

} // namespace nx::utils::file_system::test
