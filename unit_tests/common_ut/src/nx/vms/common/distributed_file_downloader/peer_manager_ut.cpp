#include <gtest/gtest.h>

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
    }

    QScopedPointer<TestPeerManager> peerManager;
};

TEST_F(DistributedFileDownloaderPeerManagerTest, invalidPeerRequest)
{
    const auto& peer = QnUuid::createUuid();

    bool called = false;
    const auto handle = peerManager->requestFileInfo(peer, "test",
        [&](bool success, rest::Handle /*handle*/, const DownloaderFileInformation& fileInfo)
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
        [&](bool success, rest::Handle /*handle*/, const DownloaderFileInformation& fileInfo)
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
        [&](bool success, rest::Handle /*handle*/, const DownloaderFileInformation& fileInfo)
    {
        ok = !success && fileInfo.status == DownloaderFileInformation::Status::notFound;
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

} // namespace test
} // namespace distributed_file_downloader
} // namespace common
} // namespace vms
} // namespace nx
