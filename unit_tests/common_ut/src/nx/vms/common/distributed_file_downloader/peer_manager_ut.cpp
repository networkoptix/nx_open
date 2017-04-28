#include <gtest/gtest.h>

#include "test_peer_manager.h"

namespace {

constexpr int kDefaultPeersPerOperation = 5;

} // namespace

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
        peerManager->setPeersPerOperation(kDefaultPeersPerOperation);
    }

    QScopedPointer<TestPeerManager> peerManager;
};

TEST_F(DistributedFileDownloaderPeerManagerTest, simpleSelection)
{
    peerManager->addPeer(QnUuid::createUuid());
    peerManager->addPeer(QnUuid::createUuid());

    auto peers = peerManager->selectPeersForOperation();
    ASSERT_EQ(peers.size(), 2);

    peerManager->addPeer(QnUuid::createUuid());
    peerManager->addPeer(QnUuid::createUuid());
    peerManager->addPeer(QnUuid::createUuid());
    peerManager->addPeer(QnUuid::createUuid());

    peers = peerManager->selectPeersForOperation();
    ASSERT_EQ(peers.size(), kDefaultPeersPerOperation);
}

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

    ASSERT_EQ(handle, 0);
    ASSERT_FALSE(called);
}

TEST_F(DistributedFileDownloaderPeerManagerTest, fileInfo)
{
    const auto& peer = QnUuid::createUuid();

    peerManager->addPeer(peer);

    DownloaderFileInformation fileInformation("test");
    peerManager->setFileInformation(peer, fileInformation);

    bool ok = false;
    peerManager->requestFileInfo(peer, fileInformation.name,
        [&](bool success, rest::Handle /*handle*/, const DownloaderFileInformation& fileInfo)
        {
            ok = success && fileInfo.name == fileInformation.name;
        });

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

    ASSERT_TRUE(ok);
}

} // namespace test
} // namespace distributed_file_downloader
} // namespace common
} // namespace vms
} // namespace nx
