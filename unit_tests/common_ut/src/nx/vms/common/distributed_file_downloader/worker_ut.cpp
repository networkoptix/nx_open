#include <gtest/gtest.h>

#include <nx/vms/common/private/distributed_file_downloader/storage.h>
#include <nx/vms/common/private/distributed_file_downloader/worker.h>

#include "utils.h"
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
        ASSERT_TRUE(QDir().mkpath(workingDirectory.absolutePath()));
    }

    TestPeerManager::FileInformation createTestFile(
        const QString& fileName = kTestFileName, qint64 size = kTestFileSize)
    {
        const QFileInfo fileInfo(workingDirectory.absoluteFilePath(fileName));
        auto directory = fileInfo.absoluteDir();
        if (!directory.exists())
        {
            if (!QDir().mkpath(directory.absolutePath()))
                return TestPeerManager::FileInformation();
        }

        if (!utils::createTestFile(fileInfo.absoluteFilePath(), size))
            return TestPeerManager::FileInformation();

        TestPeerManager::FileInformation testFileInfo(kTestFileName);
        testFileInfo.filePath = workingDirectory.absoluteFilePath(fileName);
        testFileInfo.size = kTestFileSize;
        testFileInfo.md5 = Storage::calculateMd5(testFileInfo.filePath);
        if (testFileInfo.md5.isEmpty())
            return TestPeerManager::FileInformation();

        return testFileInfo;
    }

    std::unique_ptr<Storage> createStorage(const QString& name)
    {
        if (!workingDirectory.mkpath(name))
            return nullptr;

        return std::make_unique<Storage>(workingDirectory.absoluteFilePath(name));
    }

    void addPeerWithFile(TestPeerManager* peerManager,
        const TestPeerManager::FileInformation& fileInformation)
    {
        const auto peerId = peerManager->addPeer();
        peerManager->setFileInformation(peerId, fileInformation);
    }

    QDir workingDirectory;
};

TEST_F(DistributedFileDownloaderWorkerTest, requestingFileInfo)
{
    const auto& fileInfo = createTestFile();
    ASSERT_TRUE(fileInfo.isValid());

    auto peerManager = new TestPeerManager();
    addPeerWithFile(peerManager, fileInfo);

    auto storage = createStorage("worker");
    ASSERT_TRUE(storage);
    storage->addFile(fileInfo.name);

    TestWorker worker(fileInfo.name, storage.get(), peerManager);

    worker.start();

    const auto& newFileInfo = storage->fileInformation(fileInfo.name);
    ASSERT_TRUE(newFileInfo.isValid());
    ASSERT_EQ(newFileInfo.size, fileInfo.size);
    ASSERT_TRUE(newFileInfo.md5 == fileInfo.md5);
}

TEST_F(DistributedFileDownloaderWorkerTest, simpleDownload)
{
    const auto& fileInfo = createTestFile();
    ASSERT_TRUE(fileInfo.isValid());

    auto peerManager = new TestPeerManager();
    addPeerWithFile(peerManager, fileInfo);

    auto storage = createStorage("worker");
    ASSERT_EQ(storage->addFile(fileInfo), DistributedFileDownloader::ErrorCode::noError);

    bool finished = false;

    TestWorker worker(fileInfo.name, storage.get(), peerManager);
    QObject::connect(&worker, &Worker::finished, [&finished] { finished = true; });

    worker.start();

    for (int i = 0; i < fileInfo.downloadedChunks.size() - 1; ++i)
    {
        ASSERT_FALSE(finished);
        worker.waitForNextStep(0);
    }
    ASSERT_TRUE(finished);

    const auto& newFileInfo = storage->fileInformation(fileInfo.name);
    ASSERT_TRUE(newFileInfo.isValid());
    ASSERT_EQ(newFileInfo.size, fileInfo.size);
    ASSERT_EQ(newFileInfo.md5, fileInfo.md5);
}

} // namespace test
} // namespace distributed_file_downloader
} // namespace common
} // namespace vms
} // namespace nx
