#include <recorder/storage_manager.h>
#include <test_support/mediaserver_launcher.h>
#include <test_support/storage_utils.h>

#include <map>
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

    void whenThreeStoragesAddedWithOneSystem4TimesSmaller()
    {
        std::array<int64_t, 3> totalSpaces =
            {100*1024*1024*1024LL, 150*1024*1024*1024LL, 250*1024*1024*1024LL};
        std::array<QString, 3> urls = {"url1", "url2", "url3"};
        const int64_t spaceLimit = 1*1024*1024*1024LL;

        for (size_t i = 0; i < totalSpaces.size(); ++i)
        {
            const auto newStorage = test_support::addStorageFixture(
                m_server.get(), m_server->serverModule(), urls[i], totalSpaces[i],
                totalSpaces[i], spaceLimit, /*isSystem*/i == 0, /*isOnline*/true);

            if (i == 0)
                m_smallStorage = newStorage;

            m_urlToEffectiveSpace[urls[i]].value = totalSpaces[i] - spaceLimit;
        }

        const auto totalEffectiveSpace = std::accumulate(
            m_urlToEffectiveSpace.cbegin(), m_urlToEffectiveSpace.cend(), 0LL,
            [](int64_t a, const std::pair<QString, EffectiveSpace>& p)
            {
                return a + p.second.value;
            });

        for (auto& p: m_urlToEffectiveSpace)
            p.second.ratio = static_cast<double>(p.second.value) / totalEffectiveSpace;
    }

    void whenThreeStoragesAddedWithOneSystem5TimesSmaller()
    {
        m_smallStorage = test_support::addStorageFixture(
            m_server.get(), m_server->serverModule(), "url1", /*totalSpace*/100*1024*1024*1024LL,
            /*freeSpace*/100*1024*1024*1024LL, /*spaceLimit*/1*1024*1024*1024LL, /*isSystem*/true,
            /*isOnline*/true);

        m_addedStorages.append(test_support::addStorageFixture(
            m_server.get(), m_server->serverModule(), "url2", /*totalSpace*/260*1024*1024*1024LL,
            /*freeSpace*/260*1024*1024*1024LL, /*spaceLimit*/1*1024*1024*1024LL, /*isSystem*/false,
            /*isOnline*/true));

        m_addedStorages.append(test_support::addStorageFixture(
            m_server.get(), m_server->serverModule(), "url3", /*totalSpace*/260*1024*1024*1024LL,
            /*freeSpace*/260*1024*1024*1024LL, /*spaceLimit*/1*1024*1024*1024LL, /*isSystem*/false,
            /*isOnline*/true));
    }

    void expectWritableStoragesSize(int size)
    {
        while (size != m_writableStorages.size())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            getWritableStorages();
        }
    }

    void thenSmallerShouldntAppearOnTheWritableList(int expectedNumberOfWritableStorages)
    {
        whenSomeTimeForInitializationHasPassed();
        expectWritableStoragesSize(expectedNumberOfWritableStorages);
        ASSERT_FALSE(std::any_of(
            m_writableStorages.cbegin(), m_writableStorages.cend(),
            [this](const auto& storage)
            {
                return storage == m_smallStorage;
            }));
    }

    void whenOptimalStorageIsCalledForManyTimes()
    {
        for (int i = 0; i < kIterations; ++i)
        {
            const auto optimalStorage =
                m_server->serverModule()->normalStorageManager()->getOptimalStorageRoot();
            m_urlToSelectedCount[optimalStorage->getUrl()]++;
        }
    }

    void thenTheyShouldBeSelectedProportionallyToTheirEffectiveSpaces()
    {
        for (const auto& p: m_urlToEffectiveSpace)
        {
            const double diff = std::abs<double>(
                p.second.ratio - static_cast<double>(m_urlToSelectedCount[p.first]) / kIterations);
            ASSERT_LE(diff, 0.1);
        }
    }

private:
    struct EffectiveSpace
    {
        int64_t value;
        double ratio;
    };

    std::unique_ptr<MediaServerLauncher> m_server;
    QnStorageResourceList m_addedStorages;
    QnStorageResourceList m_writableStorages;
    QnStorageResourcePtr m_smallStorage;
    std::map<QString, EffectiveSpace> m_urlToEffectiveSpace;
    std::map<QString, int> m_urlToSelectedCount;
    const int kIterations = 100000;

    void getWritableStorages()
    {
        m_writableStorages.append(
            m_server->serverModule()->normalStorageManager()->getAllWritableStorages());
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
    thenSmallerShouldntAppearOnTheWritableList(/*expectedNumberOfWritableStorages*/1);
}

TEST_F(FtWritableStorageManager, systemStorage_5timesSmaller)
{
    whenThreeStoragesAddedWithOneSystem5TimesSmaller();
    thenSmallerShouldntAppearOnTheWritableList(/*expectedNumberOfWritableStorages*/2);
}

TEST_F(FtWritableStorageManager, systemStorage_4timesSmaller)
{
    whenThreeStoragesAddedWithOneSystem4TimesSmaller();
    expectWritableStoragesSize(3);
}

TEST_F(FtWritableStorageManager, optimalStorage)
{
    whenThreeStoragesAddedWithOneSystem4TimesSmaller();
    whenOptimalStorageIsCalledForManyTimes();
    thenTheyShouldBeSelectedProportionallyToTheirEffectiveSpaces();
}

} // namespace nx::vms::server::test
