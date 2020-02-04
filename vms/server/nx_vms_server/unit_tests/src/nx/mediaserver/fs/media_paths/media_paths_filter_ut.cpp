#include <algorithm>
#include <gtest/gtest.h>
#include <nx/vms/server/fs/media_paths/media_paths_filter_config.h>
#include <nx/vms/server/fs/media_paths/media_paths.h>

namespace nx {
namespace vms::server {
namespace fs {
namespace media_paths {
namespace detail {
namespace test {

class MediaPathFilter: public ::testing::Test
{
protected:
    enum class Os
    {
        windows,
        other
    };

    void SetUp() override
    {
        m_filterConfig.serverUuid = kServerUuid;
        m_filterConfig.mediaFolderName = kMediaFolder;
        m_filterConfig.dataDirectory = kDataDirectory;
    }

    void givenWindowsPartitons()
    {
        appendPartition("C:\\", nx::vms::server::PlatformMonitor::LocalDiskPartition);
        appendPartition("D:\\", nx::vms::server::PlatformMonitor::LocalDiskPartition);
        appendPartition("E:\\", nx::vms::server::PlatformMonitor::RemovableDiskPartition);
        appendPartition(R"(\\external)", nx::vms::server::PlatformMonitor::NetworkPartition);
    }

    void givenLinuxPartitions()
    {
        appendPartition("/", nx::vms::server::PlatformMonitor::LocalDiskPartition);
        appendPartition("/mnt/external", nx::vms::server::PlatformMonitor::NetworkPartition);
        appendPartition("/mnt/flash", nx::vms::server::PlatformMonitor::RemovableDiskPartition);
    }

    void whenNonHdd(bool allowed)
    {
        whenNetwork(allowed);
        whenRemovable(allowed);
    }

    void whenNetwork(bool allowed)
    {
        m_filterConfig.isNetworkDrivesAllowed = allowed;
    }

    void whenRemovable(bool allowed)
    {
        m_filterConfig.isRemovableDrivesAllowed = allowed;
    }

    void givenOsIs(Os os)
    {
        m_filterConfig.isWindows = os == Os::windows;
        switch (os)
        {
            case Os::windows:
                m_isWindows = true;
                givenWindowsPartitons();
                break;
            case Os::other:
                givenLinuxPartitions();
                break;
        }
    }

    void whenMultipleInstancesFlagIs(bool value)
    {
        m_filterConfig.isMultipleInstancesAllowed = value;
        m_isMultipleInstances = value;
    }

    void whenPathsRequested()
    {
        m_mediaPartitions = media_paths::getMediaPartitions(m_filterConfig);
    }

    void thenNumberOfStoragesReturnedShouldBeEqualTo(int expected) const
    {
        ASSERT_EQ(expected, m_mediaPartitions.size());
    }

    void thenPathsShouldBeAmendedCorrectly() const
    {
        assertMediaFolders();
        assertSeparators();
        assertServerUuid();
        assertDataDirectory();
        assertPartitionTypes();
    }

    void assertMediaFolders() const
    {
        const int foldersWithMediaSuffixCount = static_cast<int>(std::count_if(
            m_mediaPartitions.cbegin(), m_mediaPartitions.cend(),
            [](const Partition& p) { return p.path.contains(kMediaFolder); }));

        if (m_isWindows)
        {
            ASSERT_EQ(m_mediaPartitions.size(), foldersWithMediaSuffixCount);
            return;
        }

        ASSERT_EQ(qMax(m_mediaPartitions.size() - 1, 0), foldersWithMediaSuffixCount);
    }

    void assertSeparators() const
    {
        const QString unexpectedSeparator = m_isWindows ? "/" : "\\";
        for (const auto& p: m_mediaPartitions)
            ASSERT_FALSE(p.path.contains(unexpectedSeparator));
    }

    void assertServerUuid() const
    {
        if (!m_isMultipleInstances)
            return;

        for (const auto& p: m_mediaPartitions)
            ASSERT_TRUE(p.path.contains(kServerUuid.toString()));
    }

    void assertDataDirectory() const
    {
        if (m_isWindows)
            return;

        auto pathIt = std::find_if(
            m_mediaPartitions.cbegin(),
            m_mediaPartitions.cend(),
            [](const Partition& p)
            {
                return p.path.contains(kDataDirectory);
            });

        ASSERT_NE(m_mediaPartitions.cend(), pathIt);
        ASSERT_FALSE(pathIt->path.contains(kMediaFolder));
    }

    void thenThereShouldBeNoThisPath(const QString& path) const
    {
        ASSERT_TRUE(
            std::none_of(
                m_mediaPartitions.cbegin(),
                m_mediaPartitions.cend(),
                [&path](const Partition& p)
                {
                    return p.path.contains(path);
                }));
    }

    void whenSomePathsDuplicated()
    {
        m_filterConfig.partitions.append(m_filterConfig.partitions);
    }

private:
    static const QnUuid kServerUuid;
    static const QString kMediaFolder;
    static const QString kDataDirectory;

    FilterConfig m_filterConfig;
    QList<Partition> m_mediaPartitions;
    bool m_isMultipleInstances = false;
    bool m_isWindows = false;

    void appendPartition(const QString& path, nx::vms::server::PlatformMonitor::PartitionType type)
    {
        nx::vms::server::PlatformMonitor::PartitionSpace partition;
        partition.path = path;
        partition.type = type;
        m_filterConfig.partitions.append(partition);
    }

    void assertPartitionTypes() const
    {
        for (const auto& p: m_mediaPartitions)
        {
            std::optional<nx::vms::server::PlatformMonitor::PartitionSpace> inputPartition;
            for (const auto& inp: m_filterConfig.partitions)
            {
                if (p.path.contains(inp.path) && (!inputPartition || inp.path.size() > inputPartition->path.size()))
                    inputPartition = inp;
            }

            ASSERT_TRUE(inputPartition);
            ASSERT_EQ(p.type, inputPartition->type);
        }
    }
};

const QnUuid MediaPathFilter::kServerUuid = QnUuid::fromStringSafe("{AC4F57C2-DCC1-4D56-945B-4744D19FB7CC}");
const QString MediaPathFilter::kMediaFolder = "HD Witness Media";
const QString MediaPathFilter::kDataDirectory = "/opt/networkoptix/mediaserver/var";


TEST_F(MediaPathFilter, Windows_allAllowed_noMultipleInstances)
{
    givenOsIs(Os::windows);
    whenNonHdd(true);
    whenMultipleInstancesFlagIs(false);
    whenPathsRequested();

    thenNumberOfStoragesReturnedShouldBeEqualTo(4);
    thenPathsShouldBeAmendedCorrectly();
}

TEST_F(MediaPathFilter, Windows_nonHddNotAllowed_noMultipleInstances)
{
    givenOsIs(Os::windows);
    whenNonHdd(false);
    whenMultipleInstancesFlagIs(false);
    whenPathsRequested();

    thenNumberOfStoragesReturnedShouldBeEqualTo(2);
    thenThereShouldBeNoThisPath("\\\\external");
    thenThereShouldBeNoThisPath("E");
    thenPathsShouldBeAmendedCorrectly();
}

TEST_F(MediaPathFilter, Windows_networkNotAllowed_noMultipleInstances)
{
    givenOsIs(Os::windows);
    whenNonHdd(true);
    whenNetwork(false);
    whenMultipleInstancesFlagIs(false);
    whenPathsRequested();

    thenNumberOfStoragesReturnedShouldBeEqualTo(3);
    thenThereShouldBeNoThisPath("\\\\external");
    thenPathsShouldBeAmendedCorrectly();
}

TEST_F(MediaPathFilter, Windows_removableNotAllowed_noMultipleInstances)
{
    givenOsIs(Os::windows);
    whenNonHdd(true);
    whenRemovable(false);
    whenMultipleInstancesFlagIs(false);
    whenPathsRequested();

    thenNumberOfStoragesReturnedShouldBeEqualTo(3);
    thenThereShouldBeNoThisPath("E");
    thenPathsShouldBeAmendedCorrectly();
}


TEST_F(MediaPathFilter, Windows_allAllowed_MultipleInstances)
{
    givenOsIs(Os::windows);
    whenNonHdd(true);
    whenMultipleInstancesFlagIs(true);
    whenPathsRequested();

    thenNumberOfStoragesReturnedShouldBeEqualTo(4);
    thenPathsShouldBeAmendedCorrectly();
}

TEST_F(MediaPathFilter, Linux_allAllowed_noMultipleInstances)
{
    givenOsIs(Os::other);
    whenNonHdd(true);
    whenMultipleInstancesFlagIs(false);
    whenPathsRequested();

    thenNumberOfStoragesReturnedShouldBeEqualTo(3);
    thenPathsShouldBeAmendedCorrectly();
}

TEST_F(MediaPathFilter, FilterOutNonUnique)
{
    givenOsIs(Os::other);
    whenNonHdd(true);
    whenMultipleInstancesFlagIs(false);
    whenSomePathsDuplicated();
    whenPathsRequested();

    thenNumberOfStoragesReturnedShouldBeEqualTo(3);
    thenPathsShouldBeAmendedCorrectly();
}

} // namespace test
} // namespace detail
} // namespace media_paths
} // namespace fs
} // namespace vms::server
} // namespace nx
