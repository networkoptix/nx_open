#include <gtest/gtest.h>

#include <nx/vms/common/distributed_file_downloader/private/storage.h>
#include <nx/vms/common/distributed_file_downloader/private/worker.h>

#include <test_setup.h>

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
        workingDirectory =
            QDir(TestSetup::getTemporaryDirectoryPath()).absoluteFilePath("worker_ut");
        workingDirectory.removeRecursively();
        NX_ASSERT(QDir().mkpath(workingDirectory.absolutePath()));

        commonPeerManager.reset(new TestPeerManager());
        peerManager = new ProxyTestPeerManager(commonPeerManager.data());
        storage.reset(createStorage("default"));
        worker.reset(new TestWorker(kTestFileName, storage.data(), peerManager));
    }

    virtual void TearDown() override
    {
        worker.reset();
        storage.reset();
        commonPeerManager.reset();
        NX_ASSERT(workingDirectory.removeRecursively());
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
        testFileInfo.checksums =
            Storage::calculateChecksums(testFileInfo.filePath, testFileInfo.chunkSize);
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

    void addPeerWithFile(const TestPeerManager::FileInformation& fileInformation)
    {
        const auto peerId = commonPeerManager->addPeer();
        commonPeerManager->setFileInformation(peerId, fileInformation);
    }

    void processRequests()
    {
        commonPeerManager->processRequests();
    }

    QDir workingDirectory;
    QScopedPointer<TestPeerManager> commonPeerManager;
    ProxyTestPeerManager* peerManager;
    QScopedPointer<Storage> storage;
    QScopedPointer<TestWorker> worker;
};

TEST_F(DistributedFileDownloaderWorkerTest, simplePeerSelection)
{
    constexpr int kPeersPerOperation = 5;
    worker->setPeersPerOperation(kPeersPerOperation);

    commonPeerManager->addPeer(QnUuid::createUuid());
    commonPeerManager->addPeer(QnUuid::createUuid());

    auto peers = worker->selectPeersForOperation();
    ASSERT_EQ(peers.size(), 2);

    commonPeerManager->addPeer(QnUuid::createUuid());
    commonPeerManager->addPeer(QnUuid::createUuid());
    commonPeerManager->addPeer(QnUuid::createUuid());
    commonPeerManager->addPeer(QnUuid::createUuid());

    peers = worker->selectPeersForOperation();
    ASSERT_EQ(peers.size(), kPeersPerOperation);
}

TEST_F(DistributedFileDownloaderWorkerTest, forcedPeersSelection)
{
    const QList<QnUuid> forcedPeers{QnUuid::createUuid()};

    commonPeerManager->addPeer(forcedPeers.first());
    commonPeerManager->addPeer(QnUuid::createUuid());
    commonPeerManager->addPeer(QnUuid::createUuid());
    commonPeerManager->addPeer(QnUuid::createUuid());
    commonPeerManager->addPeer(QnUuid::createUuid());

    worker->setForcedPeers(forcedPeers);
    auto peers = worker->selectPeersForOperation();
    ASSERT_EQ(peers.size(), forcedPeers.size());
}

TEST_F(DistributedFileDownloaderWorkerTest, preferredPeersSelection)
{
    const QList<QnUuid> preferredPeers{QnUuid::createUuid()};

    commonPeerManager->addPeer(preferredPeers.first());
    for (int i = 0; i < 100; ++i)
        commonPeerManager->addPeer(QnUuid::createUuid());

    worker->setPreferredPeers(preferredPeers);

    for (int i = 0; i < 5; ++i)
    {
        auto peers = worker->selectPeersForOperation();
        ASSERT_TRUE(peers.contains(preferredPeers.first()));
    }
}

TEST_F(DistributedFileDownloaderWorkerTest, neighbourPeersSelection)
{
    for (int i = 0; i < 100; ++i)
        commonPeerManager->addPeer(QnUuid::createUuid());

    auto peers = peerManager->getAllPeers();
    peers.append(peerManager->selfId());

    std::sort(peers.begin(), peers.end());

    const int selfPos = peers.indexOf(peerManager->selfId());

    const int maxDistance = (worker->peersPerOperation() + 1) / 2;

    auto distance =
        [&peers, selfPos](const QnUuid& peerId)
        {
            const int pos = peers.indexOf(peerId);
            const int dist = std::abs(selfPos - pos);
            return std::min(dist, peers.size() - dist);
        };

    const auto& selectedPeers = worker->selectPeersForOperation();
    for (const auto& peerId: selectedPeers)
        ASSERT_LE(distance(peerId), maxDistance);
}

TEST_F(DistributedFileDownloaderWorkerTest, requestingFileInfo)
{
    const auto& fileInfo = createTestFile();

    addPeerWithFile(fileInfo);
    storage->addFile(fileInfo.name);

    worker->start();
    processRequests();

    const auto& newFileInfo = storage->fileInformation(fileInfo.name);
    ASSERT_TRUE(newFileInfo.isValid());
    ASSERT_EQ(newFileInfo.size, fileInfo.size);
    ASSERT_TRUE(newFileInfo.md5 == fileInfo.md5);
}

TEST_F(DistributedFileDownloaderWorkerTest, simpleDownload)
{
    auto fileInfo = createTestFile();
    NX_ASSERT(storage->addFile(fileInfo) == ErrorCode::noError);

    fileInfo.downloadedChunks.fill(true);

    addPeerWithFile(fileInfo);

    bool finished = false;

    QObject::connect(worker.data(), &Worker::finished, [&finished] { finished = true; });

    worker->start();
    processRequests();

    const int maxSteps = fileInfo.downloadedChunks.size() + 4;

    for (int i = 0; i < maxSteps; ++i)
    {
        if (finished)
            break;

        processRequests();
    }

    ASSERT_TRUE(finished);

    const auto& newFileInfo = storage->fileInformation(fileInfo.name);
    ASSERT_TRUE(newFileInfo.isValid());
    ASSERT_EQ(newFileInfo.size, fileInfo.size);
    ASSERT_EQ(newFileInfo.md5, fileInfo.md5);
}

TEST_F(DistributedFileDownloaderWorkerTest, corruptedFile)
{
    auto fileInfo = createTestFile();
    fileInfo.downloadedChunks.setBit(0); //< Making file corrupted.

    NX_ASSERT(storage->addFile(fileInfo) == ErrorCode::noError);

    fileInfo.downloadedChunks.fill(true);

    addPeerWithFile(fileInfo);

    bool finished = false;

    QObject::connect(worker.data(), &Worker::finished, [&finished] { finished = true; });

    worker->start();
    processRequests();

    const int maxSteps = fileInfo.downloadedChunks.size() + 8;

    bool wasCorrupted = false;

    for (int i = 0; i < maxSteps; ++i)
    {
        if (finished)
            break;

        if (worker->state() == Worker::State::requestingChecksums)
        {
            ASSERT_EQ(storage->fileInformation(fileInfo.name).status,
                FileInformation::Status::corrupted);
            wasCorrupted = true;
        }

        processRequests();
    }

    ASSERT_TRUE(wasCorrupted);
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
