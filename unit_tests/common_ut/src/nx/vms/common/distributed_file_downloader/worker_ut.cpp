#include <gtest/gtest.h>

#include <nx/utils/log/log.h>
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
    using Worker::increasePeerRank;
    using Worker::decreasePeerRank;

    virtual void waitForNextStep(int delay) override
    {
        if (m_insideStep || delay >= -1) // -1 is the default value
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

        defaultPeer = createPeer();
        peerById[defaultPeer->id] = defaultPeer;

        NX_LOG(
            lm("Default peer: %1, worker: %2")
                .arg(defaultPeer->id.toString())
                .arg(defaultPeer->worker),
            cl_logINFO);
    }

    virtual void TearDown() override
    {
        qDeleteAll(peerById);
        peerById.clear();
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

    struct Peer;
    Peer* createPeer()
    {
        const auto peerId = QnUuid::createUuid();

        auto peerManager = new ProxyTestPeerManager(commonPeerManager.data(), peerId);
        auto storage = createStorage(peerId.toString());
        auto worker = new TestWorker(kTestFileName, storage, peerManager);

        return new Peer{peerId, peerManager, storage, worker};
    }

    void addPeerWithFile(const TestPeerManager::FileInformation& fileInformation)
    {
        const auto peerId = commonPeerManager->addPeer();
        commonPeerManager->setFileInformation(peerId, fileInformation);
    }

    void nextStep()
    {
        NX_LOG(lm("Step %1").arg(step), cl_logDEBUG2);

        for (const auto& peer: peerById)
        {
            const auto state = peer->worker->state();
            if (state == Worker::State::finished || state == Worker::State::failed)
                continue;

            peer->worker->waitForNextStep(-2);
        }

        commonPeerManager->processRequests();

        ++step;
    }

    QDir workingDirectory;
    QScopedPointer<TestPeerManager> commonPeerManager;

    struct Peer
    {
        QnUuid id;
        ProxyTestPeerManager* peerManager = nullptr;
        Storage* storage = nullptr;
        TestWorker* worker = nullptr;

        ~Peer()
        {
            delete worker;
            delete storage;
            // peerManager is owned and deleted by worker.
        }
    };

    QHash<QnUuid, Peer*> peerById;
    Peer* defaultPeer = nullptr;
    int step = 0;
};

TEST_F(DistributedFileDownloaderWorkerTest, simplePeersSelection)
{
    constexpr int kPeersPerOperation = 5;
    defaultPeer->worker->setPeersPerOperation(kPeersPerOperation);

    commonPeerManager->addPeer(QnUuid::createUuid());
    commonPeerManager->addPeer(QnUuid::createUuid());

    auto peers = defaultPeer->worker->selectPeersForOperation();
    ASSERT_EQ(peers.size(), 2);

    commonPeerManager->addPeer(QnUuid::createUuid());
    commonPeerManager->addPeer(QnUuid::createUuid());
    commonPeerManager->addPeer(QnUuid::createUuid());
    commonPeerManager->addPeer(QnUuid::createUuid());

    peers = defaultPeer->worker->selectPeersForOperation();
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

    defaultPeer->worker->setForcedPeers(forcedPeers);
    auto peers = defaultPeer->worker->selectPeersForOperation();
    ASSERT_EQ(peers.size(), forcedPeers.size());
}

TEST_F(DistributedFileDownloaderWorkerTest, preferredPeersSelection)
{
    const QList<QnUuid> preferredPeers{QnUuid::createUuid()};

    commonPeerManager->addPeer(preferredPeers.first());
    for (int i = 0; i < 100; ++i)
        commonPeerManager->addPeer(QnUuid::createUuid());

    defaultPeer->worker->setPreferredPeers(preferredPeers);

    for (int i = 0; i < 20; ++i)
    {
        auto peers = defaultPeer->worker->selectPeersForOperation();
        ASSERT_TRUE(peers.contains(preferredPeers.first()));
    }
}

TEST_F(DistributedFileDownloaderWorkerTest, closestGuidPeersSelection)
{
    for (int i = 0; i < 100; ++i)
        commonPeerManager->addPeer();

    auto peers = defaultPeer->peerManager->getAllPeers();
    std::sort(peers.begin(), peers.end());

    const int selfPos = std::distance(
        peers.begin(),
        std::lower_bound(peers.begin(), peers.end(), defaultPeer->peerManager->selfId()));

    const int maxDistance = (defaultPeer->worker->peersPerOperation() + 1) / 2;

    auto distance =
        [&peers, selfPos](const QnUuid& peerId)
        {
            const int pos = peers.indexOf(peerId);
            const int dist = std::abs(selfPos - pos);
            return std::min(dist, peers.size() - dist);
        };

    for (const auto& peer: peers)
        defaultPeer->worker->increasePeerRank(peer);

    const auto& selectedPeers = defaultPeer->worker->selectPeersForOperation();
    int closestGuidPeersCount = 0;
    for (const auto& peerId: selectedPeers)
    {
        if (distance(peerId) <= maxDistance)
            ++closestGuidPeersCount;
    }
    ASSERT_GE(closestGuidPeersCount, selectedPeers.size() - 1);
}

TEST_F(DistributedFileDownloaderWorkerTest, requestingFileInfo)
{
    const auto& fileInfo = createTestFile();

    addPeerWithFile(fileInfo);
    defaultPeer->storage->addFile(fileInfo.name);

    defaultPeer->worker->start();
    nextStep();

    const auto& newFileInfo = defaultPeer->storage->fileInformation(fileInfo.name);
    ASSERT_TRUE(newFileInfo.isValid());
    ASSERT_EQ(newFileInfo.size, fileInfo.size);
    ASSERT_TRUE(newFileInfo.md5 == fileInfo.md5);
}

TEST_F(DistributedFileDownloaderWorkerTest, simpleDownload)
{
    auto fileInfo = createTestFile();
    NX_ASSERT(defaultPeer->storage->addFile(fileInfo) == ErrorCode::noError);

    fileInfo.downloadedChunks.fill(true);

    addPeerWithFile(fileInfo);

    bool finished = false;

    QObject::connect(defaultPeer->worker, &Worker::finished, [&finished] { finished = true; });

    defaultPeer->worker->start();
    nextStep();

    const int maxSteps = fileInfo.downloadedChunks.size() + 4;

    for (int i = 0; i < maxSteps && !finished; ++i)
        nextStep();

    ASSERT_TRUE(finished);

    const auto& newFileInfo = defaultPeer->storage->fileInformation(fileInfo.name);
    ASSERT_TRUE(newFileInfo.isValid());
    ASSERT_EQ(newFileInfo.size, fileInfo.size);
    ASSERT_EQ(newFileInfo.md5, fileInfo.md5);
}

TEST_F(DistributedFileDownloaderWorkerTest, corruptedFile)
{
    auto fileInfo = createTestFile();
    fileInfo.downloadedChunks.setBit(0); //< Making file corrupted.

    NX_ASSERT(defaultPeer->storage->addFile(fileInfo) == ErrorCode::noError);

    fileInfo.downloadedChunks.fill(true);

    addPeerWithFile(fileInfo);

    bool finished = false;
    bool wasCorrupted = false;

    QObject::connect(defaultPeer->worker, &Worker::finished, [&finished] { finished = true; });
    QObject::connect(defaultPeer->worker, &Worker::stateChanged,
        [this, &wasCorrupted, &fileInfo]
        {
            if (defaultPeer->worker->state() == Worker::State::requestingChecksums)
            {
                ASSERT_EQ(defaultPeer->storage->fileInformation(fileInfo.name).status,
                    FileInformation::Status::corrupted);
                wasCorrupted = true;
            }
        });

    defaultPeer->worker->start();
    nextStep();

    const int maxSteps = fileInfo.downloadedChunks.size() + 8;

    for (int i = 0; i < maxSteps && !finished; ++i)
        nextStep();

    ASSERT_TRUE(wasCorrupted);
    ASSERT_TRUE(finished);

    const auto& newFileInfo = defaultPeer->storage->fileInformation(fileInfo.name);
    ASSERT_TRUE(newFileInfo.isValid());
    ASSERT_EQ(newFileInfo.size, fileInfo.size);
    ASSERT_EQ(newFileInfo.md5, fileInfo.md5);
}

TEST_F(DistributedFileDownloaderWorkerTest, simpleDownloadFromInternet)
{
    auto fileInfo = createTestFile();
    fileInfo.url = "http://test.org/testFile";
    NX_ASSERT(defaultPeer->storage->addFile(fileInfo) == ErrorCode::noError);

    commonPeerManager->setHasInternetConnection(defaultPeer->peerManager->selfId());
    commonPeerManager->addInternetFile(fileInfo.url, fileInfo.filePath);

    bool finished = false;

    QObject::connect(defaultPeer->worker, &Worker::finished, [&finished] { finished = true; });

    defaultPeer->worker->start();
    nextStep();

    const int maxSteps = fileInfo.downloadedChunks.size() + 4;

    for (int i = 0; i < maxSteps && !finished; ++i)
        nextStep();

    ASSERT_TRUE(finished);

    const auto& newFileInfo = defaultPeer->storage->fileInformation(fileInfo.name);
    ASSERT_TRUE(newFileInfo.isValid());
    ASSERT_EQ(newFileInfo.size, fileInfo.size);
    ASSERT_EQ(newFileInfo.md5, fileInfo.md5);
}

TEST_F(DistributedFileDownloaderWorkerTest, multiDownloadFlatNetwork)
{
    auto fileInfo = createTestFile();
    fileInfo.url = "http://test.org/testFile";
    commonPeerManager->addInternetFile(fileInfo.url, fileInfo.filePath);

    const QStringList groups{"default"};

    QList<QnUuid> pendingPeers;

    NX_ASSERT(defaultPeer->storage->addFile(fileInfo) == ErrorCode::noError);
    commonPeerManager->setHasInternetConnection(defaultPeer->id);
    commonPeerManager->setPeerGroups(defaultPeer->id, groups);
    commonPeerManager->setPeerStorage(defaultPeer->id, defaultPeer->storage);
    QObject::connect(defaultPeer->worker, &Worker::finished,
        [this, &pendingPeers]
        {
            pendingPeers.removeOne(defaultPeer->id);
        });

    for (int i = 0; i < 10; ++i)
    {
        auto peer = createPeer();
        peerById[peer->id] = peer;
        peer->storage->addFile(fileInfo.name);
        commonPeerManager->setPeerGroups(peer->id, groups);
        commonPeerManager->setPeerStorage(peer->id, peer->storage);

        QObject::connect(peer->worker, &Worker::finished,
            [peer, &pendingPeers]
            {
                pendingPeers.removeOne(peer->id);
            });
    }

    pendingPeers = commonPeerManager->getAllPeers();

    const int maxSteps = fileInfo.downloadedChunks.size() * 3;

    for (auto& peer: peerById)
        peer->worker->start();

    for (int i = 0; i < maxSteps && !pendingPeers.isEmpty(); ++i)
        nextStep();

    ASSERT_EQ(pendingPeers.size(), 0);
}

} // namespace test
} // namespace distributed_file_downloader
} // namespace common
} // namespace vms
} // namespace nx
