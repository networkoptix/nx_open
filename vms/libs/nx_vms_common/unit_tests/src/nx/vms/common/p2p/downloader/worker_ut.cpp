// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/log/log.h>
#include <nx/utils/test_support/test_options.h>
#include <nx/utils/random_file.h>
#include <nx/vms/common/p2p/downloader/private/storage.h>
#include <nx/vms/common/p2p/downloader/private/worker.h>

#include "test_peer_manager.h"

using namespace std::chrono;

namespace nx::vms::common::p2p::downloader::test {

namespace {

static const QString kTestFileName("test.dat");
static const qint64 kTestFileSize = 42;
static const int kChunkSize = 1;

} // namespace

class TestWorker: public Worker
{
public:
    using Worker::increasePeerRank;
    using Worker::decreasePeerRank;

    using Worker::Worker;

    virtual ~TestWorker() override
    {
        stop();
    }

private:
    virtual milliseconds delay() const override
    {
        return 3s;
    }
};

class DistributedFileDownloaderWorkerTest: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        workingDirectory.setPath(
            (nx::TestOptions::temporaryDirectoryPath().isEmpty()
                ? QDir::homePath()
                : nx::TestOptions::temporaryDirectoryPath()) + "/worker_ut");
        workingDirectory.removeRecursively();
        NX_ASSERT(QDir().mkpath(workingDirectory.absolutePath()));

        commonPeerManager.reset(new TestPeerManager());

        defaultPeer = createPeer("Default Peer");
        peerById[defaultPeer->id] = defaultPeer;

        NX_INFO(this, "Default Peer worker: %1", defaultPeer->worker);

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
        bool predeterminedContent = false,
        const QString& fileName = kTestFileName,
        qint64 size = kTestFileSize)
    {
        const QFileInfo fileInfo(workingDirectory.absoluteFilePath(fileName));
        auto directory = fileInfo.absoluteDir();
        if (!directory.exists())
        {
            if (!QDir().mkpath(directory.absolutePath()))
            {
                NX_ASSERT(false, "Cannot create directory for test file.");
                return TestPeerManager::FileInformation();
            }
        }

        if (predeterminedContent)
        {
            QFile file(fileInfo.absoluteFilePath());
            NX_ASSERT(file.open(QFile::WriteOnly));

            const QByteArray pattern = "hello";
            for (int written = 0; written != size;)
            {
                written += file.write(
                    pattern.constData(), std::min<int64_t>(pattern.size(), size - written));
            }

            file.close();
        }
        else
        {
            utils::createRandomFile(fileInfo.absoluteFilePath(), size);
        }

        TestPeerManager::FileInformation testFileInfo(kTestFileName);
        testFileInfo.filePath = workingDirectory.absoluteFilePath(fileName);
        testFileInfo.chunkSize = kChunkSize;
        testFileInfo.size = kTestFileSize;
        testFileInfo.md5 = Storage::calculateMd5(testFileInfo.filePath);
        testFileInfo.checksums =
            Storage::calculateChecksums(testFileInfo.filePath, testFileInfo.chunkSize);
        if (testFileInfo.md5.isEmpty())
        {
            NX_ASSERT(false, "Cannot calculate md5 for file.");
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
            NX_ASSERT(false, "Cannot create storage directory");
            return nullptr;
        }

        return new Storage(workingDirectory.absoluteFilePath(name));
    }

    struct Peer;
    Peer* createPeer(
        const QString& peerName = QString(), const nx::Uuid& peerId = nx::Uuid::createUuid())
    {
        auto peer = new Peer();
        peer->id = peerId;
        peer->peerManager = new ProxyTestPeerManager(commonPeerManager.data(), peerId, peerName);
        peer->storage = createStorage(peerId.toString());
        peer->worker = std::make_shared<TestWorker>(
            kTestFileName, peer->storage, QList<AbstractPeerManager*>{peer->peerManager}, selfId);
        return peer;
    }

    nx::Uuid addPeerWithFile(const TestPeerManager::FileInformation& fileInformation)
    {
        const auto peerId = commonPeerManager->addPeer();
        commonPeerManager->setFileInformation(peerId, fileInformation);
        return peerId;
    }

    QDir workingDirectory;
    QScopedPointer<TestPeerManager> commonPeerManager;
    nx::Uuid selfId = nx::Uuid::createUuid();

    struct Peer
    {
        nx::Uuid id;
        ProxyTestPeerManager* peerManager = nullptr;
        Storage* storage = nullptr;
        std::shared_ptr<TestWorker> worker;

        ~Peer()
        {
            worker.reset();
            delete storage;
            // peerManager is owned and deleted by worker.
        }
    };

    QHash<nx::Uuid, Peer*> peerById;
    Peer* defaultPeer = nullptr;
    int step = 0;
};

TEST_F(DistributedFileDownloaderWorkerTest, requestingFileInfo)
{
    const auto& fileInfo = createTestFile();

    const nx::Uuid& peerId = addPeerWithFile(fileInfo);
    defaultPeer->storage->addFile(fileInfo.name);

    std::promise<void> readyPromise;

    QObject::connect(defaultPeer->worker.get(), &Worker::stateChanged,
        [&readyPromise, previousState = defaultPeer->worker->state()](
            Worker::State state) mutable
        {
            if (previousState == Worker::State::requestingFileInformation)
                readyPromise.set_value();

            previousState = state;
        });

    defaultPeer->peerManager->setPeerList({peerId});
    defaultPeer->worker->start();

    readyPromise.get_future().wait();

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

    std::promise<void> readyPromise;
    QObject::connect(defaultPeer->worker.get(), &Worker::finished,
        [&readyPromise]() mutable { readyPromise.set_value(); });

    defaultPeer->peerManager->setPeerList(defaultPeer->peerManager->getAllPeers());
    defaultPeer->worker->start();

    readyPromise.get_future().wait();

    const auto& newFileInfo = defaultPeer->storage->fileInformation(fileInfo.name);
    ASSERT_TRUE(newFileInfo.isValid());
    ASSERT_EQ(newFileInfo.size, fileInfo.size);
    ASSERT_EQ(newFileInfo.md5, fileInfo.md5);
}

TEST_F(DistributedFileDownloaderWorkerTest, chunkDownloadFailedAndRecovered)
{
    auto fileInfo = createTestFile();
    NX_ASSERT(defaultPeer->storage->addFile(fileInfo) == ResultCode::ok);
    fileInfo.downloadedChunks.fill(true);
    addPeerWithFile(fileInfo);
    commonPeerManager->setOneShotDownloadFail();

    std::promise<void> readyPromise;
    QObject::connect(defaultPeer->worker.get(), &Worker::finished,
        [&readyPromise]() mutable { readyPromise.set_value(); });

    bool chunkFailed = false;
    QObject::connect(defaultPeer->worker.get(), &Worker::chunkDownloadFailed,
        [&chunkFailed]() { chunkFailed = true; });

    defaultPeer->peerManager->setPeerList(defaultPeer->peerManager->getAllPeers());
    defaultPeer->worker->start();

    readyPromise.get_future().wait();
    ASSERT_TRUE(chunkFailed);

    const auto& newFileInfo = defaultPeer->storage->fileInformation(fileInfo.name);
    ASSERT_TRUE(newFileInfo.isValid());
    ASSERT_EQ(newFileInfo.size, fileInfo.size);
    ASSERT_EQ(newFileInfo.md5, fileInfo.md5);
}

TEST_F(DistributedFileDownloaderWorkerTest, corruptedFile)
{
    auto fileInfo = createTestFile(true);
    fileInfo.downloadedChunks.setBit(0); //< Making file corrupted.

    NX_ASSERT(defaultPeer->storage->addFile(fileInfo) == ResultCode::ok);

    fileInfo.downloadedChunks.fill(true);

    addPeerWithFile(fileInfo);

    bool wasCorrupted = false;

    std::promise<void> readyPromise;

    QObject::connect(defaultPeer->worker.get(), &Worker::finished,
        [&readyPromise] { readyPromise.set_value(); });
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

    readyPromise.get_future().wait();
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

    commonPeerManager->setHasInternetConnection(defaultPeer->id);
    commonPeerManager->setIndirectInternetRequestsAllowed(true);
    commonPeerManager->addInternetFile(fileInfo.url, fileInfo.filePath);

    std::promise<void> readyPromise;
    QObject::connect(defaultPeer->worker.get(), &Worker::finished,
        [&readyPromise] { readyPromise.set_value(); });

    defaultPeer->peerManager->setPeerList(defaultPeer->peerManager->getAllPeers());
    defaultPeer->worker->start();

    readyPromise.get_future().wait();

    const auto& newFileInfo = defaultPeer->storage->fileInformation(fileInfo.name);
    ASSERT_TRUE(newFileInfo.isValid());
    ASSERT_EQ(newFileInfo.size, fileInfo.size);
    ASSERT_EQ(newFileInfo.md5, fileInfo.md5);
}

TEST_F(DistributedFileDownloaderWorkerTest, downloadFromInternetByProxyingRequests)
{
    auto fileInfo = createTestFile();
    fileInfo.url = "http://test.org/testFile";
    NX_ASSERT(defaultPeer->storage->addFile(fileInfo) == ResultCode::ok);

    const auto proxyPeerId = commonPeerManager->addPeer("Proxy Peer");

    commonPeerManager->setHasInternetConnection(proxyPeerId);
    commonPeerManager->setIndirectInternetRequestsAllowed(true);
    commonPeerManager->addInternetFile(fileInfo.url, fileInfo.filePath);

    std::promise<void> readyPromise;
    QObject::connect(defaultPeer->worker.get(), &Worker::finished,
        [&readyPromise] { readyPromise.set_value(); });

    defaultPeer->peerManager->setPeerList(defaultPeer->peerManager->getAllPeers());
    defaultPeer->worker->start();

    readyPromise.get_future().wait();

    const auto& newFileInfo = defaultPeer->storage->fileInformation(fileInfo.name);
    ASSERT_TRUE(newFileInfo.isValid());
    ASSERT_EQ(newFileInfo.size, fileInfo.size);
    ASSERT_EQ(newFileInfo.md5, fileInfo.md5);
}

} // namespace nx::vms::common::p2p::downloader::test
