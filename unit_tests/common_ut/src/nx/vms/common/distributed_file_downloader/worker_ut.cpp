#include <gtest/gtest.h>

#include <nx/vms/common/private/distributed_file_downloader/storage.h>
#include <nx/vms/common/private/distributed_file_downloader/worker.h>

#include "test_peer_manager.h"

namespace nx {
namespace vms {
namespace common {
namespace distributed_file_downloader {
namespace test {

namespace {

static const QString kTestFileName("test.dat");
static const qint64 kTestFileSize = 1024 * 1024 * 4 + 42;

} // namespace

class TestWorker: public Worker
{
public:
    using Worker::Worker;

    virtual void waitForNextStep(int /*delay*/) override
    {
        if (m_insideStep)
            return;

        m_insideStep = true;
        Worker::waitForNextStep(0);
        m_insideStep = false;
    }

private:
    bool m_insideStep = false;
};

class DistributedFileDownloaderWorkerTest: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        workingDirectory = QDir::temp().absoluteFilePath("__tmp_dfd_worker_test");
        workingDirectory.removeRecursively();
        ASSERT_TRUE(QDir().mkdir(workingDirectory.absolutePath()));
    }

    QDir workingDirectory;
};

TEST_F(DistributedFileDownloaderWorkerTest, requestingFileInfo)
{
    Storage storage(workingDirectory.absoluteFilePath("worker1"));
    storage.addFile(kTestFileName);

    auto peerManager = new TestPeerManager;

    TestWorker worker(kTestFileName, &storage, peerManager);
    worker.start();
}

} // namespace test
} // namespace distributed_file_downloader
} // namespace common
} // namespace vms
} // namespace nx
