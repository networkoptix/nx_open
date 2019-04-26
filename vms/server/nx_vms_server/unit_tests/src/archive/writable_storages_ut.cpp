#include <recorder/storage_manager.h>
#include <test_support/mediaserver_launcher.h>
#include <test_support/storage_utils.h>

#include <gtest/gtest.h>

namespace nx::vms::server::test {

class FtWritableStorageManager: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        m_server.reset(new MediaServerLauncher());
        ASSERT_TRUE(m_server->start());
    }

    void whenSomeOfflineStoragesAdded()
    {
        test_support::addStorageFixture(
            m_server.get(), m_server->serverModule(), "url1", /*totalSpace*/100*1024*1024*1024LL,
            /*freeSpace*/100*1024*1024*1024LL, /*spaceLimit*/1*1024*1024*1024LL, /*isSystem*/false,
            /*isOnline*/false);
    }

    void whenSomeTimeForInitializationHasPassed()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    }

    void thenWritableStoragesListShouldBeEmpty()
    {
        getWritableStorages();
        expectWritableStoragesSize(0);
    }

    void whenOneOnlineStorageAdded()
    {
        m_addedStorages.append(test_support::addStorageFixture(
            m_server.get(), m_server->serverModule(), "url1", /*totalSpace*/100*1024*1024*1024LL,
            /*freeSpace*/100*1024*1024*1024LL, /*spaceLimit*/1*1024*1024*1024LL, /*isSystem*/false,
            /*isOnline*/true));
    }

    void thenWritableStoragesListShouldContainIt()
    {
        getWritableStorages();
        expectWritableStoragesSize(1);
        while (true)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            EXPECT_EQ(m_writableStorages.size(), m_addedStorages.size());
            bool allFound = true;
            for (const auto& storage: m_addedStorages)
            {
                const bool hasStorage = std::any_of(
                    m_writableStorages.cbegin(), m_writableStorages.cend(),
                    [&storage](const auto& writableStorage)
                    {
                        return storage == writableStorage;
                    });
                if (!hasStorage)
                {
                    allFound = false;
                    break;
                }
            }
            if (allFound)
                break;
        }
    }

    void whenOneOnlineStorageWithNotEnoughFreeSpaceAdded()
    {
        m_addedStorages.append(test_support::addStorageFixture(
            m_server.get(), m_server->serverModule(), "url1", /*totalSpace*/100*1024*1024*1024LL,
            /*freeSpace*/0.9*1024*1024*1024LL, /*spaceLimit*/1*1024*1024*1024LL, /*isSystem*/false,
            /*isOnline*/true));
    }

    void whenOneOnlineStorageWithNotEnoughFreeSpaceAndNxOccupiedSpaceAdded()
    {
        const auto storageFixture = test_support::addStorageFixture(
            m_server.get(), m_server->serverModule(), "url1", /*totalSpace*/100*1024*1024*1024LL,
            /*freeSpace*/0.4*1024*1024*1024LL, /*spaceLimit*/1*1024*1024*1024LL, /*isSystem*/false,
            /*isOnline*/true);
        m_addedStorages.append(storageFixture);
        test_support::setNxOccupiedSpace(
            m_server.get(), storageFixture, /*nxOccupiedSpace*/0.5*1024*1024*1024LL);
    }

    void whenOneOnlineStorageWithNotEnoughFreeSpaceButWithEnoughNxOccupiedSpaceAdded()
    {
        const auto storageFixture = test_support::addStorageFixture(
            m_server.get(), m_server->serverModule(), "url1", /*totalSpace*/100*1024*1024*1024LL,
            /*freeSpace*/0.1*1024*1024*1024LL, /*spaceLimit*/1*1024*1024*1024LL, /*isSystem*/false,
            /*isOnline*/true);
        m_addedStorages.append(storageFixture);
        test_support::setNxOccupiedSpace(
            m_server.get(), storageFixture, /*nxOccupiedSpace*/1.2*1024*1024*1024LL);
    }

    void whenTwoStoragesWithEnoughFreeSpaceWith10TimesSpaceDifferenceAdded()
    {
        m_smallStorage = test_support::addStorageFixture(
            m_server.get(), m_server->serverModule(), "url1", /*totalSpace*/100*1024*1024*1024LL,
            /*freeSpace*/100*1024*1024*1024LL, /*spaceLimit*/1*1024*1024*1024LL, /*isSystem*/false,
            /*isOnline*/true);

        m_addedStorages.append(test_support::addStorageFixture(
            m_server.get(), m_server->serverModule(), "url2", /*totalSpace*/1500*1024*1024*1024LL,
            /*freeSpace*/1500*1024*1024*1024LL, /*spaceLimit*/1*1024*1024*1024LL, /*isSystem*/false,
            /*isOnline*/true));
    }

    void thenSmallerShouldntAppearOnTheWritableList()
    {
        whenSomeTimeForInitializationHasPassed();
        expectWritableStoragesSize(1);
        ASSERT_FALSE(std::any_of(
            m_writableStorages.cbegin(), m_writableStorages.cend(),
            [this](const auto& storage)
            {
                return storage == m_smallStorage;
            }));
    }


private:
    std::unique_ptr<MediaServerLauncher> m_server;
    QnStorageResourceList m_addedStorages;
    QnStorageResourceList m_writableStorages;
    QnStorageResourcePtr m_smallStorage;

    void getWritableStorages()
    {
        m_writableStorages.append(
            m_server->serverModule()->normalStorageManager()->getAllWritableStorages());
    }

    void expectWritableStoragesSize(int size)
    {
        while (size != m_writableStorages.size())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            getWritableStorages();
        }
    }
};

TEST_F(FtWritableStorageManager, noOfflineStorages)
{
    whenSomeOfflineStoragesAdded();
    whenSomeTimeForInitializationHasPassed();
    thenWritableStoragesListShouldBeEmpty();
}


TEST_F(FtWritableStorageManager, oneOnlineStorage)
{
    whenOneOnlineStorageAdded();
    thenWritableStoragesListShouldContainIt();
}

TEST_F(FtWritableStorageManager, spaceLessStorages_noFreeSpace)
{
    whenOneOnlineStorageWithNotEnoughFreeSpaceAdded();
    whenSomeTimeForInitializationHasPassed();
    thenWritableStoragesListShouldBeEmpty();
}

TEST_F(FtWritableStorageManager, spaceLessStorages_noNxOccupiedSpace)
{
    whenOneOnlineStorageWithNotEnoughFreeSpaceAndNxOccupiedSpaceAdded();
    thenWritableStoragesListShouldBeEmpty();
}

TEST_F(FtWritableStorageManager, noFreeSpace_butNxOccupiedSpaceIsEnough)
{
    whenOneOnlineStorageWithNotEnoughFreeSpaceButWithEnoughNxOccupiedSpaceAdded();
    thenWritableStoragesListShouldContainIt();
}

TEST_F(FtWritableStorageManager, tenTimesSmallerStorage)
{
    whenTwoStoragesWithEnoughFreeSpaceWith10TimesSpaceDifferenceAdded();
    thenSmallerShouldntAppearOnTheWritableList();
}

} // namespace nx::vms::server::test
