#include <nx/vms/server/fs/partitions/read_partitions_linux.h>
#include <nx/vms/server/fs/partitions/abstract_partitions_information_provider_linux.h>
#include <gtest/gtest.h>
#include <string.h>
#include <QtCore>

namespace nx::vms::server::fs::test {

namespace {

void assertDecodedStrings(QByteArray source, const QByteArray& desired)
{
    decodeOctalEncodedPath(source.data());
    ASSERT_EQ(0, strcmp(source.constData(), desired.constData()));
}

class TestPartitionsInformationProvider: public AbstractPartitionsInformationProvider
{
public:
    TestPartitionsInformationProvider():
        m_initialMountsFileContent(R"(sysfs /sys sysfs rw,nosuid,nodev,noexec,relatime 0 0
proc /proc proc rw,nosuid,nodev,noexec,relatime 0 0
udev /dev devtmpfs rw,nosuid,relatime,size=6680584k,nr_inodes=1670146,mode=755 0 0
devpts /dev/pts devpts rw,nosuid,noexec,relatime,gid=5,mode=620,ptmxmode=000 0 0
tmpfs /run tmpfs rw,nosuid,noexec,relatime,size=1342172k,mode=755 0 0
/dev/sda1 /a/long/long/long/path ext4 rw,relatime,errors=remount-ro,data=ordered 0 0
securityfs /sys/kernel/security securityfs rw,nosuid,nodev,noexec,relatime 0 0
tmpfs /dev/shm tmpfs rw,nosuid,nodev 0 0
tmpfs /run/lock tmpfs rw,nosuid,nodev,noexec,relatime,size=5120k 0 0
tmpfs /sys/fs/cgroup tmpfs rw,mode=755 0 0
cgroup /sys/fs/cgroup/systemd cgroup rw,nosuid,nodev,noexec,relatime,xattr,release_agent=/lib/systemd/systemd-cgroups-agent,name=systemd 0 0
pstore /sys/fs/pstore pstore rw,nosuid,nodev,noexec,relatime 0 0
cgroup /sys/fs/cgroup/blkio cgroup rw,nosuid,nodev,noexec,relatime,blkio 0 0
cgroup /sys/fs/cgroup/pids cgroup rw,nosuid,nodev,noexec,relatime,pids,release_agent=/run/cgmanager/agents/cgm-release-agent.pids 0 0
cgroup /sys/fs/cgroup/cpu,cpuacct cgroup rw,nosuid,nodev,noexec,relatime,cpu,cpuacct 0 0
cgroup /sys/fs/cgroup/cpuset cgroup rw,nosuid,nodev,noexec,relatime,cpuset,clone_children 0 0
cgroup /sys/fs/cgroup/rdma cgroup rw,nosuid,nodev,noexec,relatime,rdma,release_agent=/run/cgmanager/agents/cgm-release-agent.rdma 0 0
cgroup /sys/fs/cgroup/devices cgroup rw,nosuid,nodev,noexec,relatime,devices 0 0
cgroup /sys/fs/cgroup/memory cgroup rw,nosuid,nodev,noexec,relatime,memory 0 0
cgroup /sys/fs/cgroup/perf_event cgroup rw,nosuid,nodev,noexec,relatime,perf_event,release_agent=/run/cgmanager/agents/cgm-release-agent.perf_event 0 0
cgroup /sys/fs/cgroup/net_cls,net_prio cgroup rw,nosuid,nodev,noexec,relatime,net_cls,net_prio 0 0
cgroup /sys/fs/cgroup/hugetlb cgroup rw,nosuid,nodev,noexec,relatime,hugetlb,release_agent=/run/cgmanager/agents/cgm-release-agent.hugetlb 0 0
cgroup /sys/fs/cgroup/freezer cgroup rw,nosuid,nodev,noexec,relatime,freezer 0 0
systemd-1 /proc/sys/fs/binfmt_misc autofs rw,relatime,fd=26,pgrp=1,timeout=0,minproto=5,maxproto=5,direct,pipe_ino=727 0 0
debugfs /sys/kernel/debug debugfs rw,relatime 0 0
mqueue /dev/mqueue mqueue rw,relatime 0 0
hugetlbfs /dev/hugepages hugetlbfs rw,relatime,pagesize=2M 0 0
fusectl /sys/fs/fuse/connections fusectl rw,relatime 0 0
configfs /sys/kernel/config configfs rw,relatime 0 0
binfmt_misc /proc/sys/fs/binfmt_misc binfmt_misc rw,relatime 0 0
cgmfs /run/cgmanager/fs tmpfs rw,relatime,size=100k,mode=755 0 0
tmpfs /run/user/1000 tmpfs rw,nosuid,nodev,relatime,size=1342172k,mode=700,uid=1000,gid=1000 0 0
gvfsd-fuse /run/user/1000/gvfs fuse.gvfsd-fuse rw,nosuid,nodev,relatime,user_id=1000,group_id=1000 0 0
/dev/sda1 /etc/hosts ext4 rw,relatime,errors=remount-ro,data=ordered 0 0
)")
    {}

    void returnInvalidSpaceForPath(const QList<QString>& subPaths)
    {
        m_invalidSpaceSubPaths = subPaths;
    }

    void replaceMountsLineByPattern(const QByteArray& pattern, const QByteArray& newLine)
    {
        QBuffer buf(&m_initialMountsFileContent);
        buf.open(QIODevice::ReadOnly);
        QByteArray newContent;

        for (QByteArray line = buf.readLine(); !line.isNull(); line = buf.readLine())
        {
            if (line.contains(pattern))
                newContent.append(newLine + '\n');
            else
                newContent.append(line);
        }

        m_initialMountsFileContent = newContent;
    }

    virtual QByteArray mountsFileContent() const
    {
        return m_initialMountsFileContent;
    }

    virtual bool isScanfLongPattern() const
    {
        return false;
    }

    virtual qint64 totalSpace(const QByteArray& fsPath) const
    {
        if (hasInvalidSpaceSubPathMatch(fsPath))
            return -1;
        return 1;
    }

    virtual qint64 freeSpace(const QByteArray& fsPath) const
    {
        if (hasInvalidSpaceSubPathMatch(fsPath))
            return -1;
        return 1;
    }

    virtual bool isFolder(const QByteArray& fsPath) const
    {
        if (fsPath.contains("etc"))
            return false;
        return true;
    }

private:
    QList<QString> m_invalidSpaceSubPaths;
    QByteArray m_initialMountsFileContent;

    bool hasInvalidSpaceSubPathMatch(const QString& fsPath) const
    {
        return std::any_of(m_invalidSpaceSubPaths.cbegin(),
            m_invalidSpaceSubPaths.cend(),
            [&fsPath](const QString& subPath)
            {
                return fsPath.contains(subPath);
            });
    }
};

} // namespace

TEST(decodeOctalEncodedPath, variousCases)
{
    assertDecodedStrings("abcdef", "abcdef");
    assertDecodedStrings("ab\040cdef", "ab cdef");
    assertDecodedStrings("ab\40cdef", "ab\40cdef");
    assertDecodedStrings("abcdef\0", "abcdef\0");
    assertDecodedStrings("/ab\040cd/ab\040cd/ab", "/ab cd/ab cd/ab");
    assertDecodedStrings(
        "/a/long/long/very\040long/with\040spaces",
        "/a/long/long/very long/with spaces");
}

class ReadPartitions: public ::testing::Test
{
protected:
    void whenProcMountsContainingBoundFilesRead()
    {
        readPartitionsInformation(&m_partitionsInformationProvider, &m_partitionInfoList);
    }

    void thenOnlyFoldersOnDeviceShouldBeSelectedAsStoragePaths()
    {
        ASSERT_TRUE(std::any_of(m_partitionInfoList.cbegin(), m_partitionInfoList.cend(),
            [](const auto& pInfo)
            {
                return pInfo.devName == "/dev/sda1" && pInfo.path == "/a/long/long/long/path";
            }));

        ASSERT_TRUE(std::none_of(m_partitionInfoList.cbegin(), m_partitionInfoList.cend(),
            [](const auto& pInfo) { return pInfo.path.contains("etc"); }));
    }

    void whenImpossibleToObtainSpaceForSda1Paths()
    {
        m_partitionsInformationProvider.returnInvalidSpaceForPath({"hosts", "long/long"});
    }

    void thenThesePathsShouldBeExcludedFromTheResults()
    {
        ASSERT_TRUE(std::none_of(m_partitionInfoList.cbegin(), m_partitionInfoList.cend(),
            [](const auto& pInfo) { return pInfo.devName == "/dev/sda1"; }));

        ASSERT_TRUE(std::none_of(m_partitionInfoList.cbegin(), m_partitionInfoList.cend(),
            [](const auto& pInfo)
            {
                return pInfo.path.contains("hosts") || pInfo.path.contains("long");
            }));
    }

    void whenThereIsASuitablePathWithSpacesOnSda1()
    {
        m_partitionsInformationProvider.replaceMountsLineByPattern("long/long",
            R"(/dev/sda1 /a/long/long/very\040long/with\040spaces ext4 rw,relatime,errors=remount-ro,data=ordered 0 0)");
    }

    void thenPathWithSpacesShouldBeSelected()
    {
        ASSERT_TRUE(std::any_of(m_partitionInfoList.cbegin(), m_partitionInfoList.cend(),
            [](const auto& pInfo)
            {
                return pInfo.devName == "/dev/sda1"
                    && pInfo.path == "/a/long/long/very long/with spaces";
            }));
    }

private:
    TestPartitionsInformationProvider m_partitionsInformationProvider;
    std::list<PartitionInfo> m_partitionInfoList;
};

TEST_F(ReadPartitions, FilterOutNonFolders)
{
    whenProcMountsContainingBoundFilesRead();
    thenOnlyFoldersOnDeviceShouldBeSelectedAsStoragePaths();
}

TEST_F(ReadPartitions, InvalidSpacePathsShouldNotBeReturned)
{
    whenImpossibleToObtainSpaceForSda1Paths();
    whenProcMountsContainingBoundFilesRead();
    thenThesePathsShouldBeExcludedFromTheResults();
}

TEST_F(ReadPartitions, PathsWithSpacesShouldBeReturned)
{
    whenThereIsASuitablePathWithSpacesOnSda1();
    whenProcMountsContainingBoundFilesRead();
    thenPathWithSpacesShouldBeSelected();
}

} // namespace nx::vms::server::fs::test
