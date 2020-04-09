#include <plugins/storage/file_storage/file_storage_resource.h>

#include <gtest/gtest.h>

#include <test_support/mediaserver_launcher.h>
#include <nx/vms/server/fs/media_paths/media_paths_filter_config.h>

class CheckMountedStatusTest: public ::testing::Test
{
protected:
    QnFileStorageResourcePtr storage;
    QString canonicalPathArgument;

    virtual void SetUp() override
    {
        m_server = std::make_unique<MediaServerLauncher>();
        ASSERT_TRUE(m_server->start());


        storage = QnFileStorageResourcePtr(new QnFileStorageResource(m_server->serverModule()));
        storage->getMockableCallFactory()->setCanonicalPathFunc(
            [this](const QString& path)
            {
                canonicalPathArgument = path;
                if (path == "/opt/networkoptx/mediaserver/var"
                    || path == "/existing/path"
                    || path == "/existing/pa")
                {
                    return path;
                }

                return QString("");
            });
    }

    void mockMediaPathFilter(
        QList<PlatformMonitor::PartitionSpace> additional = QList<PlatformMonitor::PartitionSpace>())
    {
        nx::vms::server::fs::media_paths::FilterConfig filterConfig;
        filterConfig.isWindows = false;

        auto partition = PlatformMonitor::PartitionSpace(
            "/", /*freeSpace*/100'000'000'000, /*totalSpace*/100'000'000'000);
        partition.type = PlatformMonitor::LocalDiskPartition;
        additional.push_back(partition);

        filterConfig.partitions = additional;
        filterConfig.serverUuid = m_server->commonModule()->moduleGUID();
        filterConfig.dataDirectory = "/opt/networkoptx/mediaserver/var";
        filterConfig.mediaFolderName = "Nx Witness";
        filterConfig.isNetworkDrivesAllowed = true;
        filterConfig.isRemovableDrivesAllowed = true;
        filterConfig.isMultipleInstancesAllowed = false;

        nx::vms::server::fs::media_paths::FilterConfig::setDefault(filterConfig);
    }

    virtual void TearDown() override
    {
        nx::vms::server::fs::media_paths::FilterConfig::setDefault(std::nullopt);
    }

    auto callCheckMounted() { return storage->checkMountedStatus(); }

private:
    std::unique_ptr<MediaServerLauncher> m_server;
};

TEST_F(CheckMountedStatusTest, FirstServerRun_FullMediaPathDoesNotExistYet)
{
    mockMediaPathFilter();
    storage->setUrl("/opt/networkoptx/mediaserver/var/data");

    ASSERT_EQ(Qn::StorageInit_Ok, callCheckMounted());
    #if !defined (Q_OS_WIN)
        ASSERT_EQ("/opt/networkoptx/mediaserver/var", canonicalPathArgument);
    #endif
}

TEST_F(CheckMountedStatusTest, StorageWithNotExistentPathInDb)
{
    mockMediaPathFilter();
    storage->setUrl("/not-existing/path");
    ASSERT_NE(Qn::StorageInit_Ok, callCheckMounted());
}

TEST_F(CheckMountedStatusTest, PathExistsOnDiskButItsNotMounted)
{
    mockMediaPathFilter();
    storage->setUrl("/existing/path/NX Witness");
    ASSERT_NE(Qn::StorageInit_Ok, callCheckMounted());
    #if !defined (Q_OS_WIN)
        ASSERT_EQ("/existing/path", canonicalPathArgument);
    #endif
}

TEST_F(CheckMountedStatusTest, SimilarPathInMounted)
{
    auto partition = PlatformMonitor::PartitionSpace(
        "/existing/path", /*freeSpace*/100'000'000'000, /*totalSpace*/100'000'000'000);
    partition.type = PlatformMonitor::LocalDiskPartition;
    mockMediaPathFilter({partition});

    storage->setUrl("/existing/pa/NX Witness");
    ASSERT_NE(Qn::StorageInit_Ok, callCheckMounted());
    #if !defined (Q_OS_WIN)
        ASSERT_EQ("/existing/pa", canonicalPathArgument);
    #endif
}
