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
static const qint64 kTestFileSize = 42;
static const int kChunkSize = 4;

} // namespace

class TestWorker: public Worker
{
public:
    using Worker::Worker;
    using Worker::selectPeersForOperation;

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
        NX_ASSERT(QDir().mkpath(workingDirectory.absolutePath()));

        peerManager = new TestPeerManager();
        storage.reset(createStorage("default"));
        worker.reset(new TestWorker(kTestFileName, storage.data(), peerManager));
    }

    virtual void TearDown() override
    {
        worker.reset();
        storage.reset();
    }

    TestPeerManager::FileInformation createTestFile(
        const QString& fileName = kTestFileName, qint64 size = kTestFileSize)
    {
        const QFileInfo fileInfo(workingDirectory.absoluteFilePath(fileName));
        auto directory = fileInfo.absoluteDir();
        if (!directory.exists())
        {
            if (!QDir().mkpath(directory.absolutePath()))
            {
                NX_ASSERT("Cannot create directory for test file.");
                return TestPeerManager::FileInformation();
            }
        }

        utils::createTestFile(fileInfo.absoluteFilePath(), size);

        TestPeerManager::FileInformation testFileInfo(kTestFileName);
        testFileInfo.filePath = workingDirectory.absoluteFilePath(fileName);
        testFileInfo.chunkSize = kChunkSize;
        testFileInfo.size = kTestFileSize;
        testFileInfo.md5 = Storage::calculateMd5(testFileInfo.filePath);
        if (testFileInfo.md5.isEmpty())
        {
            NX_ASSERT("Cannot calculate md5 for file.");
            return TestPeerManager::FileInformation();
        }
        testFileInfo.downloadedChunks.resize(
            Storage::calculateChunkCount(testFileInfo.size, testFileInfo.chunkSize));

        return testFileInfo;
    }

    Storage* createStorage(const QString& name)
    {
        if (!workingDirectory.mkpath(name))
        {
            NX_ASSERT("Cannot create storage directory");
            return nullptr;
        }

        return new Storage(workingDirectory.absoluteFilePath(name));
    }

    void addPeerWithFile(TestPeerManager* peerManager,
        const TestPeerManager::FileInformation& fileInformation)
    {
        const auto peerId = peerManager->addPeer();
        peerManager->setFileInformation(peerId, fileInformation);
    }

    QDir workingDirectory;
    TestPeerManager* peerManager;
    QScopedPointer<Storage> storage;
    QScopedPointer<TestWorker> worker;
};

TEST_F(DistributedFileDownloaderWorkerTest, simplePeerSelection)
{
    constexpr int kPeersPerOperation = 5;
    worker->setPeersPerOperation(kPeersPerOperation);

    peerManager->addPeer(QnUuid::createUuid());
    peerManager->addPeer(QnUuid::createUuid());

    auto peers = worker->selectPeersForOperation();
    ASSERT_EQ(peers.size(), 2);

    peerManager->addPeer(QnUuid::createUuid());
    peerManager->addPeer(QnUuid::createUuid());
    peerManager->addPeer(QnUuid::createUuid());
    peerManager->addPeer(QnUuid::createUuid());

    peers = worker->selectPeersForOperation();
    ASSERT_EQ(peers.size(), kPeersPerOperation);
}

TEST_F(DistributedFileDownloaderWorkerTest, requestingFileInfo)
{
    const auto& fileInfo = createTestFile();

    addPeerWithFile(peerManager, fileInfo);
    storage->addFile(fileInfo.name);

    worker->start();
    peerManager->processRequests();

    const auto& newFileInfo = storage->fileInformation(fileInfo.name);
    ASSERT_TRUE(newFileInfo.isValid());
    ASSERT_EQ(newFileInfo.size, fileInfo.size);
    ASSERT_TRUE(newFileInfo.md5 == fileInfo.md5);
}

TEST_F(DistributedFileDownloaderWorkerTest, simpleDownload)
{
    auto fileInfo = createTestFile();
    NX_ASSERT(storage->addFile(fileInfo) == DistributedFileDownloader::ErrorCode::noError);

    fileInfo.downloadedChunks.fill(
        true, Storage::calculateChunkCount(fileInfo.size, fileInfo.chunkSize));

    addPeerWithFile(peerManager, fileInfo);

    bool finished = false;

    QObject::connect(worker.data(), &Worker::finished, [&finished] { finished = true; });

    worker->start();
    peerManager->processRequests();

    for (int i = 0; i < fileInfo.downloadedChunks.size(); ++i)
    {
        ASSERT_FALSE(finished);
        peerManager->processRequests();
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
