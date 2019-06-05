#include <gtest/gtest.h>

#include <nx/utils/test_support/test_options.h>
#include <nx/utils/random_file.h>
#include <nx/vms/common/p2p/downloader/private/storage.h>

#include "test_peer_manager.h"

using namespace std::chrono;

namespace nx::vms::common::p2p::downloader::test {

class DistributedFileDownloaderPeerManagerTest: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        peerManager.reset(new TestPeerManager());

        storageDir =
            (nx::utils::TestOptions::temporaryDirectoryPath().isEmpty()
                ? QDir::homePath()
                : nx::utils::TestOptions::temporaryDirectoryPath()) + "/storage";
        storageDir.removeRecursively();
        NX_ASSERT(QDir().mkpath(storageDir.absolutePath()));

        peerManager->start();
    }

    virtual void TearDown() override
    {
        peerManager->start();
        NX_ASSERT(storageDir.removeRecursively());
    }

    QScopedPointer<TestPeerManager> peerManager;
    QDir storageDir;
};

TEST_F(DistributedFileDownloaderPeerManagerTest, invalidPeerRequest)
{
    const auto& peer = QnUuid::createUuid();
    auto request = peerManager->requestFileInfo(peer, "test", nx::utils::Url());
    ASSERT_FALSE(request.future.valid());
}

TEST_F(DistributedFileDownloaderPeerManagerTest, cancellingRequest)
{
    const auto& peer = peerManager->addPeer();
    peerManager->setDelayBeforeRequest(1000);

    auto request = peerManager->requestFileInfo(peer, "test", nx::utils::Url());
    request.cancel();
    request.future.wait_for(100ms);
    ASSERT_FALSE(request.future.get().has_value());
}

TEST_F(DistributedFileDownloaderPeerManagerTest, fileInfo)
{
    const auto& peer = QnUuid::createUuid();

    peerManager->addPeer(peer);

    TestPeerManager::FileInformation fileInformation("test");
    peerManager->setFileInformation(peer, fileInformation);

    auto request = peerManager->requestFileInfo(peer, fileInformation.name, fileInformation.url);
    ASSERT_TRUE(request.future.get().has_value());
}

TEST_F(DistributedFileDownloaderPeerManagerTest, emptyFileInfo)
{
    const auto& peer = QnUuid::createUuid();

    peerManager->addPeer(peer);

    auto request = peerManager->requestFileInfo(peer, "test", nx::utils::Url());
    const auto& info = request.future.get();
    ASSERT_FALSE(info.has_value());
}

TEST_F(DistributedFileDownloaderPeerManagerTest, invalidChunk)
{
    const auto& peer = QnUuid::createUuid();

    peerManager->addPeer(peer);

    TestPeerManager::FileInformation fileInformation("test");
    fileInformation.size = 1024;
    fileInformation.chunkSize = 1024;
    peerManager->setFileInformation(peer, fileInformation);

    auto request = peerManager->downloadChunk(peer, fileInformation.name, nx::utils::Url(), 2, 0);
    ASSERT_FALSE(request.future.get().has_value());
}

TEST_F(DistributedFileDownloaderPeerManagerTest, usingStorage)
{
    const QString fileName("test");
    const QString filePath(storageDir.absoluteFilePath(fileName));

    utils::createRandomFile(filePath, 1);

    auto storage = new Storage(storageDir);

    const auto& peer = peerManager->addPeer();
    peerManager->setPeerStorage(peer, storage);

    FileInformation originalFileInfo("test");
    originalFileInfo.status = FileInformation::Status::downloaded;
    NX_ASSERT(storage->addFile(originalFileInfo) == ResultCode::ok);

    originalFileInfo = storage->fileInformation(fileName);
    const auto originalChecksums = storage->getChunkChecksums(fileName);

    auto infoRequest = peerManager->requestFileInfo(peer, fileName, nx::utils::Url());
    std::optional<TestPeerManager::FileInformation> fileInfo = infoRequest.future.get();

    ASSERT_TRUE(fileInfo.has_value());
    ASSERT_TRUE(fileInfo->isValid());
    ASSERT_EQ(fileInfo->status, originalFileInfo.status);
    ASSERT_EQ(fileInfo->size, originalFileInfo.size);
    ASSERT_EQ(fileInfo->md5, originalFileInfo.md5);

    auto chunkRequest = peerManager->downloadChunk(peer, fileName, nx::utils::Url(), 0, 0);
    auto chunk = chunkRequest.future.get();

    ASSERT_TRUE(chunk.has_value());
    ASSERT_EQ(chunk->size(), originalFileInfo.size);

    auto checksumsRequest = peerManager->requestChecksums(peer, fileName);
    const auto checksums = checksumsRequest.future.get();

    ASSERT_TRUE(checksums);
    ASSERT_EQ(checksums.value(), originalChecksums);
}

TEST_F(DistributedFileDownloaderPeerManagerTest, internetFile)
{
    const QString fileName("test");
    const QString filePath(storageDir.absoluteFilePath(fileName));
    const QString url("http://test.org/test");

    utils::createRandomFile(filePath, 1);

    peerManager->addInternetFile(url, filePath);

    const auto peerId = peerManager->addPeer();
    peerManager->setHasInternetConnection(peerId);
    peerManager->setIndirectInternetRequestsAllowed(true);

    auto request = peerManager->downloadChunk(peerId, fileName, url, 0, 1);
    auto chunk = request.future.get();

    ASSERT_TRUE(chunk.has_value());
    ASSERT_EQ(chunk->size(), 1);
}

TEST_F(DistributedFileDownloaderPeerManagerTest, calculateDistances)
{
    QString groupA("A");
    QString groupB("B");
    QString groupC("C");
    QString groupD("D");

    ProxyTestPeerManager testPeerManager(peerManager.data());
    const auto& peerA = QnUuid();
    peerManager->setPeerGroups(peerA, { groupA });
    const auto& peerA2 = peerManager->addPeer();
    peerManager->setPeerGroups(peerA2, { groupA });
    const auto& peerAB = peerManager->addPeer();
    peerManager->setPeerGroups(peerAB, { groupA, groupB });
    const auto& peerB = peerManager->addPeer();
    peerManager->setPeerGroups(peerB, { groupB });
    const auto& peerBC = peerManager->addPeer();
    peerManager->setPeerGroups(peerBC, { groupB, groupC });
    const auto& peerC = peerManager->addPeer();
    peerManager->setPeerGroups(peerC, { groupC });
    const auto& peerD = peerManager->addPeer();
    peerManager->setPeerGroups(peerD, { groupD });

    testPeerManager.calculateDistances();

    ASSERT_EQ(testPeerManager.distanceTo(peerA), 0);
    ASSERT_EQ(testPeerManager.distanceTo(peerA2), 1);
    ASSERT_EQ(testPeerManager.distanceTo(peerAB), 1);
    ASSERT_EQ(testPeerManager.distanceTo(peerB), 2);
    ASSERT_EQ(testPeerManager.distanceTo(peerBC), 2);
    ASSERT_EQ(testPeerManager.distanceTo(peerC), 3);
    ASSERT_EQ(testPeerManager.distanceTo(peerD), std::numeric_limits<int>::max());
}

} // namespace nx::vms::common::p2p::downloader::test
