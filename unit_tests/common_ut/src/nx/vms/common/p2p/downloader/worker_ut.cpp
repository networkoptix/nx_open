#include <gtest/gtest.h>

#include <nx/utils/log/log.h>
#include <nx/utils/test_support/test_options.h>
#include <nx/utils/random_file.h>
#include <nx/utils/std/future.h>
#include <nx/utils/move_only_func.h>
#include <nx/vms/common/p2p/downloader/private/storage.h>
#include <nx/vms/common/p2p/downloader/private/worker.h>

#include "test_peer_manager.h"

namespace nx {
namespace vms {
namespace common {
namespace p2p {
namespace downloader {
namespace test {

namespace {

static const QString kTestFileName("test.dat");
static const qint64 kTestFileSize = 42;
static const int kChunkSize = 1;

} // namespace

class TestWorker: public Worker
{
public:
    using Worker::selectPeersForOperation;
    using Worker::increasePeerRank;
    using Worker::decreasePeerRank;

    TestWorker(
        const QString& fileName,
        Storage* storage,
        AbstractPeerManager* peerManager,
        QObject* parent = nullptr)
        :
        Worker(fileName, storage, peerManager, parent)
    {
        setPrintSelfPeerInLogs();
    }

    ~TestWorker()
    {
        stop();
    }

private:
    virtual qint64 delayMs() const override
    {
        return 3 * 1000;
    }
};

class DistributedFileDownloaderWorkerTest: public ::testing::Test, public TestPeerManagerHandler
{
protected:
    virtual void SetUp() override
    {
        workingDirectory =
            (nx::utils::TestOptions::temporaryDirectoryPath().isEmpty()
                ? QDir::homePath()
                : nx::utils::TestOptions::temporaryDirectoryPath()) + "/worker_ut";
        workingDirectory.removeRecursively();
        NX_ASSERT(QDir().mkpath(workingDirectory.absolutePath()));

        commonPeerManager.reset(new TestPeerManager(this));

        defaultPeer = createPeer("Default Peer");
        peerById[defaultPeer->id] = defaultPeer;

        NX_LOG(lm("Default Peer worker: %1").arg(defaultPeer->worker), cl_logINFO);

        commonPeerManager->start();
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

        utils::createRandomFile(fileInfo.absoluteFilePath(), size);

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
    Peer* createPeer(
        const QString& peerName = QString(), const QnUuid& peerId = QnUuid::createUuid())
    {
        auto peer = new Peer();
        peer->id = peerId;
        peer->peerManager = new ProxyTestPeerManager(commonPeerManager.data(), peerId, peerName);
        peer->storage = createStorage(peerId.toString());
        peer->worker = std::make_shared<TestWorker>(kTestFileName, peer->storage, peer->peerManager);
        return peer;
    }

    QnUuid addPeerWithFile(const TestPeerManager::FileInformation& fileInformation)
    {
        const auto peerId = commonPeerManager->addPeer();
        commonPeerManager->setFileInformation(peerId, fileInformation);
        return peerId;
    }

    QDir workingDirectory;
    QScopedPointer<TestPeerManager> commonPeerManager;

    struct Peer
    {
        QnUuid id;
        ProxyTestPeerManager* peerManager = nullptr;
        Storage* storage = nullptr;
        std::shared_ptr<TestWorker> worker;

        ~Peer()
        {
            worker->stop();
            delete storage;
            // peerManager is owned and deleted by worker.
        }
    };

    void setOnRequestFileInfoCb(nx::utils::MoveOnlyFunc<void()> requestFileInfoCb)
    {
        m_onRequestFileInfoCb = std::move(requestFileInfoCb);
    }

    virtual void onRequestFileInfo()
    {
        if (m_onRequestFileInfoCb)
            m_onRequestFileInfoCb();
    }

    QHash<QnUuid, Peer*> peerById;
    Peer* defaultPeer = nullptr;
    int step = 0;
    nx::utils::MoveOnlyFunc<void()> m_onRequestFileInfoCb = nullptr;
};

TEST_F(DistributedFileDownloaderWorkerTest, simplePeersSelection)
{
    const QList<QnUuid> peers{QnUuid::createUuid()};

    commonPeerManager->addPeer(peers.first());
    commonPeerManager->addPeer(QnUuid::createUuid());
    commonPeerManager->addPeer(QnUuid::createUuid());
    commonPeerManager->addPeer(QnUuid::createUuid());
    commonPeerManager->addPeer(QnUuid::createUuid());
    defaultPeer->peerManager->setPeerList(peers);

    auto selectedPeers = defaultPeer->worker->selectPeersForOperation();
    ASSERT_EQ(selectedPeers.size(), peers.size());
}

TEST_F(DistributedFileDownloaderWorkerTest, preferredPeersSelection)
{
    const QList<QnUuid> preferredPeers{QnUuid::createUuid()};

    commonPeerManager->addPeer(preferredPeers.first());
    for (int i = 0; i < 100; ++i)
        commonPeerManager->addPeer(QnUuid::createUuid());

    defaultPeer->peerManager->setPeerList(defaultPeer->peerManager->getAllPeers());
    defaultPeer->worker->setPreferredPeers(preferredPeers);

    for (int i = 0; i < 20; ++i)
    {
        auto peers = defaultPeer->worker->selectPeersForOperation();
        ASSERT_TRUE(peers.contains(preferredPeers.first()));
    }
}

TEST_F(DistributedFileDownloaderWorkerTest, closestIdPeersSelection)
{
    for (int i = 0; i < 100; ++i)
        commonPeerManager->addPeer();

    auto peers = defaultPeer->peerManager->getAllPeers();
    defaultPeer->peerManager->setPeerList(peers);
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
    int closestIdPeersCount = 0;
    for (const auto& peerId: selectedPeers)
    {
        if (distance(peerId) <= maxDistance)
            ++closestIdPeersCount;
    }
    ASSERT_GE(closestIdPeersCount, selectedPeers.size() - 1);
}

TEST_F(DistributedFileDownloaderWorkerTest, requestingFileInfo)
{
    const auto& fileInfo = createTestFile();

    addPeerWithFile(fileInfo);
    defaultPeer->storage->addFile(fileInfo.name);

    nx::utils::promise<void> readyPromise;
    auto readyFuture = readyPromise.get_future();
    setOnRequestFileInfoCb(
        [readyPromise = std::move(readyPromise)]() mutable { readyPromise.set_value(); });

    defaultPeer->peerManager->setPeerList(defaultPeer->peerManager->getAllPeers());
    defaultPeer->worker->start();

    readyFuture.wait();

    const auto& newFileInfo = defaultPeer->storage->fileInformation(fileInfo.name);
    ASSERT_TRUE(newFileInfo.isValid());
    ASSERT_EQ(newFileInfo.size, fileInfo.size);
    ASSERT_TRUE(newFileInfo.md5 == fileInfo.md5);
}

TEST_F(DistributedFileDownloaderWorkerTest, simpleDownload)
{
    auto fileInfo = createTestFile();
    NX_ASSERT(defaultPeer->storage->addFile(fileInfo) == ResultCode::ok);
    fileInfo.downloadedChunks.fill(true);
    addPeerWithFile(fileInfo);

    bool finished = false;
    defaultPeer->peerManager->setPeerList(defaultPeer->peerManager->getAllPeers());
    defaultPeer->worker->start();

    nx::utils::promise<bool> readyPromise;
    auto readyFuture = readyPromise.get_future();
    QObject::connect(defaultPeer->worker.get(), &Worker::finished,
        [&readyPromise]() mutable { readyPromise.set_value(true); });

    ASSERT_TRUE(readyFuture.get());

    const auto& newFileInfo = defaultPeer->storage->fileInformation(fileInfo.name);
    ASSERT_TRUE(newFileInfo.isValid());
    ASSERT_EQ(newFileInfo.size, fileInfo.size);
    ASSERT_EQ(newFileInfo.md5, fileInfo.md5);
}


TEST_F(DistributedFileDownloaderWorkerTest, corruptedFile)
{
    auto fileInfo = createTestFile();
    fileInfo.downloadedChunks.setBit(0); //< Making file corrupted.

    NX_ASSERT(defaultPeer->storage->addFile(fileInfo) == ResultCode::ok);

    fileInfo.downloadedChunks.fill(true);

    addPeerWithFile(fileInfo);

    bool wasCorrupted = false;

    nx::utils::promise<bool> readyPromise;
    auto readyFuture = readyPromise.get_future();

    QObject::connect(defaultPeer->worker.get(), &Worker::finished,
        [&readyPromise] { readyPromise.set_value(true); });
    QObject::connect(defaultPeer->worker.get(), &Worker::stateChanged,
        [this, &wasCorrupted, &fileInfo]
        {
            if (defaultPeer->worker->state() == Worker::State::requestingChecksums)
            {
                ASSERT_EQ(defaultPeer->storage->fileInformation(fileInfo.name).status,
                    FileInformation::Status::corrupted);
                wasCorrupted = true;
            }
        });

    defaultPeer->peerManager->setPeerList(defaultPeer->peerManager->getAllPeers());
    defaultPeer->worker->start();

    ASSERT_TRUE(readyFuture.get());
    ASSERT_TRUE(wasCorrupted);

    const auto& newFileInfo = defaultPeer->storage->fileInformation(fileInfo.name);
    ASSERT_TRUE(newFileInfo.isValid());
    ASSERT_EQ(newFileInfo.size, fileInfo.size);
    ASSERT_EQ(newFileInfo.md5, fileInfo.md5);
}

TEST_F(DistributedFileDownloaderWorkerTest, simpleDownloadFromInternet)
{
    auto fileInfo = createTestFile();
    fileInfo.url = "http://test.org/testFile";
    NX_ASSERT(defaultPeer->storage->addFile(fileInfo) == ResultCode::ok);

    commonPeerManager->setHasInternetConnection(defaultPeer->peerManager->selfId());
    commonPeerManager->addInternetFile(fileInfo.url, fileInfo.filePath);

    nx::utils::promise<bool> readyPromise;
    auto readyFuture = readyPromise.get_future();
    QObject::connect(defaultPeer->worker.get(), &Worker::finished,
        [&readyPromise] { readyPromise.set_value(true); });

    defaultPeer->peerManager->setPeerList(defaultPeer->peerManager->getAllPeers());
    defaultPeer->worker->start();

    ASSERT_TRUE(readyFuture.get());

    const auto& newFileInfo = defaultPeer->storage->fileInformation(fileInfo.name);
    ASSERT_TRUE(newFileInfo.isValid());
    ASSERT_EQ(newFileInfo.size, fileInfo.size);
    ASSERT_EQ(newFileInfo.md5, fileInfo.md5);
}

TEST_F(DistributedFileDownloaderWorkerTest, downloadFromInternetViaNonSetPeer)
{
    auto fileInfo = createTestFile();
    fileInfo.url = "http://test.org/testFile";
    NX_ASSERT(defaultPeer->storage->addFile(fileInfo) == ResultCode::ok);

    const auto proxyPeerId = commonPeerManager->addPeer("Proxy Peer");

    commonPeerManager->setHasInternetConnection(proxyPeerId);
    commonPeerManager->addInternetFile(fileInfo.url, fileInfo.filePath);

    nx::utils::promise<bool> readyPromise;
    auto readyFuture = readyPromise.get_future();
    QObject::connect(defaultPeer->worker.get(), &Worker::finished,
        [&readyPromise] { readyPromise.set_value(true); });

    defaultPeer->worker->start();

    ASSERT_TRUE(readyFuture.get());

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
    nx::utils::promise<void> readyPromise;
    auto readyFuture = readyPromise.get_future();

    NX_ASSERT(defaultPeer->storage->addFile(fileInfo) == ResultCode::ok);
    commonPeerManager->setHasInternetConnection(defaultPeer->id);
    commonPeerManager->setPeerGroups(defaultPeer->id, groups);
    commonPeerManager->setPeerStorage(defaultPeer->id, defaultPeer->storage);
    QObject::connect(defaultPeer->worker.get(), &Worker::finished,
        [this, &pendingPeers, &readyPromise]
        {
            pendingPeers.removeOne(defaultPeer->id);
            if (pendingPeers.isEmpty())
                readyPromise.set_value();
        });

    const auto& baseId = QUuid::createUuid();

    QList<Peer*> peers{defaultPeer};

    for (int i = 1; i <= 10; ++i)
    {
        auto peer = createPeer(lit("Peer %1").arg(i), QnUuid::createUuidFromPool(baseId, i));
        peers.append(peer);
        peerById[peer->id] = peer;
        peer->storage->addFile(fileInfo);
        commonPeerManager->setPeerGroups(peer->id, groups);
        commonPeerManager->setPeerStorage(peer->id, peer->storage);

        QObject::connect(peer->worker.get(), &Worker::finished,
            [peer, &pendingPeers, &readyPromise]
            {
                pendingPeers.removeOne(peer->id);
                if (pendingPeers.isEmpty())
                    readyPromise.set_value();
            });
    }

    pendingPeers = commonPeerManager->getAllPeers();

    for (const auto& peer: peerById)
    {
        peer->peerManager->setPeerList(peer->peerManager->getAllPeers());
        peer->peerManager->calculateDistances();
    }

    const int maxRequests = fileInfo.downloadedChunks.size() * pendingPeers.size() * 3;

    for (auto& peer: peerById)
        peer->worker->start();

    readyFuture.wait();

    for (auto& peer: peers)
    {
        peer->peerManager->requestCounter()->printCounters(
            "Outgoing requests from " + commonPeerManager->peerString(peer->id),
            commonPeerManager.data());
    }

    commonPeerManager->requestCounter()->printCounters(
        "Incoming Requests:", commonPeerManager.data());

    ASSERT_EQ(pendingPeers.size(), 0);
    ASSERT_LE(commonPeerManager->requestCounter()->totalRequests(), maxRequests);
}

TEST_F(DistributedFileDownloaderWorkerTest, multiDownloadNonFlatNetwork)
{
    auto fileInfo = createTestFile();
    fileInfo.url = "http://test.org/testFile";
    commonPeerManager->addInternetFile(fileInfo.url, fileInfo.filePath);

    const QStringList groups{"A", "B", "C", "D", "E"};

    QList<QnUuid> pendingPeers;
    nx::utils::promise<void> readyPromise;
    auto readyFuture = readyPromise.get_future();

    NX_ASSERT(defaultPeer->storage->addFile(fileInfo) == ResultCode::ok);
    commonPeerManager->setHasInternetConnection(defaultPeer->id);
    commonPeerManager->setPeerGroups(defaultPeer->id, {groups[0]});
    commonPeerManager->setPeerStorage(defaultPeer->id, defaultPeer->storage);
    QObject::connect(defaultPeer->worker.get(), &Worker::finished,
        [this, &pendingPeers, &readyPromise]
        {
            pendingPeers.removeOne(defaultPeer->id);
            if (pendingPeers.isEmpty())
                readyPromise.set_value();
        });

    QList<Peer*> peers{defaultPeer};

    constexpr int kPeersPerGroup = 3;

    for (int groupIndex = 0; groupIndex < groups.size(); ++groupIndex)
    {
        const auto& group = groups[groupIndex];

        for (int i = 1; i <= kPeersPerGroup; ++i)
        {
            auto peer = createPeer(lit("Peer %1%2").arg(group).arg(i));
            peers.append(peer);
            peerById[peer->id] = peer;
            peer->storage->addFile(fileInfo);
            commonPeerManager->setPeerStorage(peer->id, peer->storage);

            QStringList peerGroups{groups[groupIndex]};
            if (i == kPeersPerGroup && groupIndex < groups.size() - 1)
                peerGroups.append(groups[groupIndex + 1]);
            commonPeerManager->setPeerGroups(peer->id, peerGroups);

            QObject::connect(peer->worker.get(), &Worker::finished,
                [peer, &pendingPeers, &readyPromise]
                {
                    pendingPeers.removeOne(peer->id);
                    if (pendingPeers.isEmpty())
                        readyPromise.set_value();
                });
        }
    }

    pendingPeers = commonPeerManager->getAllPeers();

    for (const auto& peer: peerById)
    {
        peer->peerManager->setPeerList(peer->peerManager->getAllPeers());
        peer->peerManager->calculateDistances();
    }

    const int maxRequests = fileInfo.downloadedChunks.size() * pendingPeers.size() * 3;

    for (auto& peer: peerById)
        peer->worker->start();

    readyFuture.wait();

    for (auto& peer: peers)
    {
        peer->peerManager->requestCounter()->printCounters(
            "Outgoing requests from " + commonPeerManager->peerString(peer->id),
            commonPeerManager.data());
    }

    commonPeerManager->requestCounter()->printCounters(
        "Incoming Requests:", commonPeerManager.data());

    ASSERT_EQ(pendingPeers.size(), 0);
    ASSERT_LE(commonPeerManager->requestCounter()->totalRequests(), maxRequests);
}

} // namespace test
} // namespace downloader
} // namespace p2p
} // namespace common
} // namespace vms
} // namespace nx
