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
        storage->getDependencyFactory()->setCanonicalPathFunc(
            [this](const QString& path)
            {
                canonicalPathArgument = path;
                if (path == "/opt/networkoptx/mediaserver/var" || path == "/existing/path")
                    return path;

                return QString("");
            });


        nx::vms::server::fs::media_paths::FilterConfig filterConfig;
        filterConfig.isWindows = false;

        auto partition = PlatformMonitor::PartitionSpace(
            "/", /*freeSpace*/100'000'000'000, /*totalSpace*/100'000'000'000);
        partition.type = PlatformMonitor::LocalDiskPartition;

        filterConfig.partitions = { partition };
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
    storage->setUrl("/opt/networkoptx/mediaserver/var/data");

    ASSERT_EQ(Qn::StorageInit_Ok, callCheckMounted());
    ASSERT_EQ("/opt/networkoptx/mediaserver/var", canonicalPathArgument);
}

TEST_F(CheckMountedStatusTest, StorageWithNotExistentPathInDb)
{
    storage->setUrl("/not-existing/path");
    ASSERT_NE(Qn::StorageInit_Ok, callCheckMounted());
}

TEST_F(CheckMountedStatusTest, PathExistsOnDiskButItsNotMounted)
{
    storage->setUrl("/existing/path/NX Witness");
    ASSERT_NE(Qn::StorageInit_Ok, callCheckMounted());
    ASSERT_EQ("/existing/path", canonicalPathArgument);
}
