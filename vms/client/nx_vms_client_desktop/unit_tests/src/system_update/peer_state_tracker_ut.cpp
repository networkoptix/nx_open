#include "client_update_test_environment.h"

#include "nx/vms/client/desktop/system_update/peer_state_tracker.h"

namespace {
    auto kDefaultServerVersion = nx::utils::SoftwareVersion("4.0.0.29067");
}

namespace nx::vms::client::desktop {

using Status = nx::update::Status::Code;
using Error = nx::update::Status::ErrorCode;

class UpdatePeerStateTrackerTest: public ClientUpdateTestEnvironment
{
public:
    // virtual void SetUp() will be called before each test is run.
    virtual void SetUp() override
    {
        ClientUpdateTestEnvironment::SetUp();
        m_stateTracker.reset(new PeerStateTracker());
    }

    // virtual void TearDown() will be called after each test is run.
    virtual void TearDown() override
    {
        m_stateTracker.reset();
        ClientUpdateTestEnvironment::TearDown();
    }

    QScopedPointer<PeerStateTracker> m_stateTracker;

    void setServerUpdateStatus(QnMediaServerResourcePtr server, Status status)
    {
        std::map<QnUuid, nx::update::Status> statusData;
        auto id = server->getId();
        statusData[id] = nx::update::Status(id, status, Error::noError);
        m_stateTracker->setUpdateStatus(statusData);
    }

    void setTask(QnMediaServerResourceList servers)
    {
        QSet<QnUuid> peers;
        for (const auto& server: servers)
            peers.insert(server->getId());
        m_stateTracker->setTask(peers);
    }
};

TEST_F(UpdatePeerStateTrackerTest, testReadyInstallTracking)
{
    ASSERT_TRUE(m_stateTracker->setResourceFeed(resourcePool()));
    auto server1 = makeServer(kDefaultServerVersion);
    auto server2 = makeServer(kDefaultServerVersion);

    setServerUpdateStatus(server1, Status::readyToInstall);
    setServerUpdateStatus(server2, Status::readyToInstall);

    setTask({server1, server2});
    m_stateTracker->processReadyInstallTaskSet();

    // Both servers should be 'active'.
    EXPECT_EQ(m_stateTracker->peersActive(), m_stateTracker->peersIssued());
    EXPECT_EQ(m_stateTracker->peersActive().size(), 2);

    // Only one server should be active now.
    server1->setStatus(Qn::Offline);
    m_stateTracker->processReadyInstallTaskSet();
    EXPECT_EQ(m_stateTracker->peersActive().size(), 1);
}

}
