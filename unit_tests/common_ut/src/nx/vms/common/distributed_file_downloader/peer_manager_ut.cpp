#include <gtest/gtest.h>

#include <nx/vms/common/distributed_file_downloader/private/storage.h>
#include <test_setup.h>

#include "test_peer_manager.h"
#include "utils.h"

namespace nx {
namespace vms {
namespace common {
namespace distributed_file_downloader {
namespace test {

class DistributedFileDownloaderPeerManagerTest: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        peerManager.reset(new TestPeerManager());

        storageDir = TestSetup::getTemporaryDirectoryPath() + "/storage";
        storageDir.removeRecursively();
        NX_ASSERT(QDir().mkpath(storageDir.absolutePath()));
    }

    virtual void TearDown() override
    {
        NX_ASSERT(storageDir.removeRecursively());
    }

    QScopedPointer<TestPeerManager> peerManager;
    QDir storageDir;
};

TEST_F(DistributedFileDownloaderPeerManagerTest, invalidPeerRequest)
{
    const auto& peer = QnUuid::createUuid();

    bool called = false;
    const auto handle = peerManager->requestFileInfo(peer, "test",
        [&](bool, rest::Handle, const FileInformation&)
        {
            // Should not be called.
            called = true;
        });

    peerManager->processRequests();

    ASSERT_EQ(handle, 0);
    ASSERT_FALSE(called);
}

TEST_F(DistributedFileDownloaderPeerManagerTest, fileInfo)
{
    const auto& peer = QnUuid::createUuid();

    peerManager->addPeer(peer);

    TestPeerManager::FileInformation fileInformation("test");
    peerManager->setFileInformation(peer, fileInformation);

    bool ok = false;
    peerManager->requestFileInfo(peer, fileInformation.name,
        [&](bool success, rest::Handle /*handle*/, const FileInformation& fileInfo)
        {
            ok = success && fileInfo.name == fileInformation.name;
        });

    peerManager->processRequests();

    ASSERT_TRUE(ok);
}

TEST_F(DistributedFileDownloaderPeerManagerTest, emptyFileInfo)
{
    const auto& peer = QnUuid::createUuid();

    peerManager->addPeer(peer);

    bool ok = false;
    peerManager->requestFileInfo(peer, "test",
        [&](bool success, rest::Handle /*handle*/, const FileInformation& fileInfo)
    {
        ok = !success && fileInfo.status == FileInformation::Status::notFound;
    });

    peerManager->processRequests();

    ASSERT_TRUE(ok);
}

TEST_F(DistributedFileDownloaderPeerManagerTest, invalidChunk)
{
    const auto& peer = QnUuid::createUuid();

    peerManager->addPeer(peer);

    TestPeerManager::FileInformation fileInformation("test");
    fileInformation.size = 1024;
    fileInformation.chunkSize = 1024;
    peerManager->setFileInformation(peer, fileInformation);

    bool ok = true;
    peerManager->downloadChunk(peer, fileInformation.name, 2,
        [&](bool success, rest::Handle /*handle*/, const QByteArray& data)
        {
            ok = success && !data.isEmpty();
        });

    peerManager->processRequests();

    ASSERT_FALSE(ok);
}

TEST_F(DistributedFileDownloaderPeerManagerTest, usingStorage)
{
    const QString fileName("test");
    const QString filePath(storageDir.absoluteFilePath(fileName));

    utils::createTestFile(filePath, 1);

    auto storage = new Storage(storageDir);

    const auto& peer = peerManager->addPeer();
    peerManager->setPeerStorage(peer, storage);

    FileInformation originalFileInfo("test");
    originalFileInfo.status = FileInformation::Status::downloaded;
    NX_ASSERT(storage->addFile(originalFileInfo) == ErrorCode::noError);

    originalFileInfo = storage->fileInformation(fileName);
    const auto originalChecksums = storage->getChunkChecksums(fileName);

    bool fileInfoReceived = false;
    TestPeerManager::FileInformation fileInfo;

    peerManager->requestFileInfo(peer, fileName,
        [&](bool success, rest::Handle /*handle*/, const FileInformation& peerFileInfo)
        {
            fileInfoReceived = success;
            fileInfo = peerFileInfo;
        });
    peerManager->processRequests();

    ASSERT_TRUE(fileInfoReceived);
    ASSERT_TRUE(fileInfo.isValid());
    ASSERT_EQ(fileInfo.status, originalFileInfo.status);
    ASSERT_EQ(fileInfo.size, originalFileInfo.size);
    ASSERT_EQ(fileInfo.md5, originalFileInfo.md5);

    bool chunkDownloaded = false;
    QByteArray chunkData;
    peerManager->downloadChunk(peer, fileName, 0,
        [&](bool success, rest::Handle /*handle*/, const QByteArray& data)
        {
            chunkDownloaded = success;
            chunkData = data;
        });
    peerManager->processRequests();

    ASSERT_TRUE(chunkDownloaded);
    ASSERT_EQ(chunkData.size(), originalFileInfo.size);

    bool checksumsReceived = false;
    QVector<QByteArray> checksums;

    peerManager->requestChecksums(peer, fileName,
        [&](bool success, rest::Handle /*handle*/, const QVector<QByteArray>& fileChecksums)
        {
            checksumsReceived = success;
            checksums = fileChecksums;
        });
    peerManager->processRequests();

    ASSERT_TRUE(checksumsReceived);
    ASSERT_EQ(checksums, originalChecksums);
}

TEST_F(DistributedFileDownloaderPeerManagerTest, calculateDistances)
{
    QString groupA("A");
    QString groupB("B");
    QString groupC("C");
    QString groupD("D");

    ProxyTestPeerManager testPeerManager(peerManager.data());
    const auto& peerA = testPeerManager.selfId();
    peerManager->setPeerGroups(peerA, {groupA});
    const auto& peerA2 = peerManager->addPeer();
    peerManager->setPeerGroups(peerA2, {groupA});
    const auto& peerAB = peerManager->addPeer();
    peerManager->setPeerGroups(peerAB, {groupA, groupB});
    const auto& peerB = peerManager->addPeer();
    peerManager->setPeerGroups(peerB, {groupB});
    const auto& peerBC = peerManager->addPeer();
    peerManager->setPeerGroups(peerBC, {groupB, groupC});
    const auto& peerC = peerManager->addPeer();
    peerManager->setPeerGroups(peerC, {groupC});
    const auto& peerD = peerManager->addPeer();
    peerManager->setPeerGroups(peerD, {groupD});

    testPeerManager.calculateDistances();

    ASSERT_EQ(testPeerManager.distanceTo(peerA), 0);
    ASSERT_EQ(testPeerManager.distanceTo(peerA2), 1);
    ASSERT_EQ(testPeerManager.distanceTo(peerAB), 1);
    ASSERT_EQ(testPeerManager.distanceTo(peerB), 2);
    ASSERT_EQ(testPeerManager.distanceTo(peerBC), 2);
    ASSERT_EQ(testPeerManager.distanceTo(peerC), 3);
    ASSERT_EQ(testPeerManager.distanceTo(peerD), std::numeric_limits<int>::max());
}

} // namespace test
} // namespace distributed_file_downloader
} // namespace common
} // namespace vms
} // namespace nx
