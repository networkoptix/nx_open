#include <algorithm>
#include <gtest/gtest.h>
#include <nx/mediaserver/fs/media_paths/media_paths_filter_config.h>
#include <nx/mediaserver/fs/media_paths/media_paths.h>

namespace nx {
namespace mediaserver {
namespace fs {
namespace media_paths {
namespace detail {
namespace test {

class Filter: public ::testing::Test
{
protected:
    enum TypesNotAllowed
    {
        none,
        network = 1,
        removable = 1 << 1
    };

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
        appendPartition("C:\\", QnPlatformMonitor::LocalDiskPartition);
        appendPartition("D:\\", QnPlatformMonitor::LocalDiskPartition);
        appendPartition("E:\\", QnPlatformMonitor::RemovableDiskPartition);
        appendPartition(R"(\\external)", QnPlatformMonitor::NetworkPartition);
    }

    void givenLinuxPartitions()
    {
        appendPartition("/", QnPlatformMonitor::LocalDiskPartition);
        appendPartition("/mnt/external", QnPlatformMonitor::NetworkPartition);
        appendPartition("/mnt/flash", QnPlatformMonitor::RemovableDiskPartition);
    }

    void whenTypesNotAllowed(int typeMask)
    {
        m_filterConfig.isNetworkDrivesAllowed = NetworkDrives::allowed;
        m_filterConfig.isRemovableDrivesAllowed = true;

        if (typeMask == TypesNotAllowed::none)
            return;

        if (typeMask & TypesNotAllowed::network)
            m_filterConfig.isNetworkDrivesAllowed = NetworkDrives::notAllowed;

        if (typeMask & TypesNotAllowed::removable)
            m_filterConfig.isRemovableDrivesAllowed = false;
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
        m_mediaPaths = media_paths::get(std::move(m_filterConfig));
    }

    void thenNumberOfStoragesReturnedShouldBeEqualTo(int expected) const
    {
        ASSERT_EQ(expected, m_mediaPaths.size());
    }

    void thenPathsShouldBeAmendedCorrectly() const
    {
        assertMediaFolders();
        assertSeparators();
        assertServerUuid();
        assertDataDirectory();
    }

    void assertMediaFolders() const
    {
        const int foldersWithMediaSuffixCount = static_cast<int>(std::count_if(
            m_mediaPaths.cbegin(),
            m_mediaPaths.cend(),
            [](const QString& path)
            {
                return path.contains(kMediaFolder);
            }));

        if (m_isWindows)
        {
            ASSERT_EQ(m_mediaPaths.size(), foldersWithMediaSuffixCount);
            return;
        }

        ASSERT_EQ(qMax(m_mediaPaths.size() - 1, 0), foldersWithMediaSuffixCount);
    }

    void assertSeparators() const
    {
        const QString unexpectedSeparator = m_isWindows ? "/" : "\\";
        for (const auto& mediaPath: m_mediaPaths)
            ASSERT_FALSE(mediaPath.contains(unexpectedSeparator));
    }

    void assertServerUuid() const
    {
        if (!m_isMultipleInstances)
            return;

        for (const auto& path: m_mediaPaths)
            ASSERT_TRUE(path.contains(kServerUuid.toString()));
    }

    void assertDataDirectory() const
    {
        if (m_isWindows)
            return;

        auto pathIt = std::find_if(
            m_mediaPaths.cbegin(),
            m_mediaPaths.cend(),
            [](const QString& path)
            {
                return path.contains(kDataDirectory);
            });

        ASSERT_NE(m_mediaPaths.cend(), pathIt);
        ASSERT_FALSE(pathIt->contains(kMediaFolder));
    }

    void thenThereShouldBeNoThisPath(const QString& path) const
    {
        ASSERT_TRUE(
            std::none_of(
                m_mediaPaths.cbegin(),
                m_mediaPaths.cend(),
                [&path](const QString& mediaPath)
                {
                    return mediaPath.contains(path);
                }));
    }

private:
    static const QnUuid kServerUuid;
    static const QString kMediaFolder;
    static const QString kDataDirectory;

    FilterConfig m_filterConfig;
    QList<QString> m_mediaPaths;
    bool m_isMultipleInstances = false;
    bool m_isWindows = false;

    void appendPartition(const QString& path, QnPlatformMonitor::PartitionType type)
    {
        QnPlatformMonitor::PartitionSpace partition;
        partition.path = path;
        partition.type = type;
        m_filterConfig.partitions.append(partition);
    }
};

const QnUuid Filter::kServerUuid = QnUuid::fromStringSafe("{AC4F57C2-DCC1-4D56-945B-4744D19FB7CC}");
const QString Filter::kMediaFolder = "HD Witness Media";
const QString Filter::kDataDirectory = "/opt/networkoptix/mediaserver/var";


TEST_F(Filter, Windows_allAllowed_noMultipleInstances)
{
    givenOsIs(Os::windows);
    whenTypesNotAllowed(TypesNotAllowed::none);
    whenMultipleInstancesFlagIs(false);
    whenPathsRequested();

    thenNumberOfStoragesReturnedShouldBeEqualTo(4);
    thenPathsShouldBeAmendedCorrectly();
}

TEST_F(Filter, Windows_networkNotAllowed_noMultipleInstances)
{
    givenOsIs(Os::windows);
    whenTypesNotAllowed(TypesNotAllowed::network);
    whenMultipleInstancesFlagIs(false);
    whenPathsRequested();

    thenNumberOfStoragesReturnedShouldBeEqualTo(3);
    thenThereShouldBeNoThisPath("\\\\external");
    thenPathsShouldBeAmendedCorrectly();
}

TEST_F(Filter, Windows_removableNotAllowed_noMultipleInstances)
{
    givenOsIs(Os::windows);
    whenTypesNotAllowed(TypesNotAllowed::removable);
    whenMultipleInstancesFlagIs(false);
    whenPathsRequested();

    thenNumberOfStoragesReturnedShouldBeEqualTo(3);
    thenThereShouldBeNoThisPath("E:\\");
    thenPathsShouldBeAmendedCorrectly();
}

TEST_F(Filter, Windows_allAllowed_MultipleInstances)
{
    givenOsIs(Os::windows);
    whenTypesNotAllowed(TypesNotAllowed::none);
    whenMultipleInstancesFlagIs(true);
    whenPathsRequested();

    thenNumberOfStoragesReturnedShouldBeEqualTo(4);
    thenPathsShouldBeAmendedCorrectly();
}

TEST_F(Filter, Linux_allAllowed_noMultipleInstances)
{
    givenOsIs(Os::other);
    whenTypesNotAllowed(TypesNotAllowed::none);
    whenMultipleInstancesFlagIs(false);
    whenPathsRequested();

    thenNumberOfStoragesReturnedShouldBeEqualTo(3);
    thenPathsShouldBeAmendedCorrectly();
}

} // namespace test
} // namespace detail
} // namespace media_paths
} // namespace fs
} // namespace mediaserver
} // namespace nx
