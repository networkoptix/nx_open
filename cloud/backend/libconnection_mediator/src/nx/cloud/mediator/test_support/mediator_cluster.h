#pragma once

#include <nx/cloud/mediator/mediator_service.h>
#include <nx/cloud/mediator/listening_peer_db.h>
#include <nx/cloud/mediator/test_support/mediator_functional_test.h>
#include <nx/cloud/discovery/test_support/discovery_server.h>

namespace nx::hpm::test {
/**
 * Provides support for mediators running in "cluster" mode.
 */
class MediatorCluster
{
public:
    MediatorCluster();

    /**
     * Adds a mediator to the cluster with necessary arguments for cluster operation.
     * NOTE: Does NOT call startAndWaitUntilStarted() on the mediator to allow further processing
     * by the user.
     */
    MediatorFunctionalTest& addMediator(
        int flags = MediatorFunctionalTest::MediatorTestFlags::allFlags,
        const QString& testDir = QString());

    /**
     * Adds a mediator to the cluster with necessary arguments for cluster operation.
     * NOTE: Does NOT call startAndWaitUntilStarted() on the mediator to allow further processing
     * by the user.
     */
    MediatorFunctionalTest& addMediator(
        std::vector <const char*> args,
        int flags = MediatorFunctionalTest::MediatorTestFlags::allFlags,
        const QString& testDir = QString());

    /*
     * Get the number of nodes (mediators) in the cluster
     */
    int size() const;

    MediatorFunctionalTest& mediator(int index);
    const MediatorFunctionalTest& mediator(int index) const;

    /**
     * Returns true if ALL Mediator's have peerDomainName.
     */
    bool peerInformationSynchronizedInCluster(const std::string& peerDomainName) const;

    /**
     * Returns true if ALL Mediator's do NOT have peerDomainName.
     */
    bool peerInformationIsAbsentFromCluster(const std::string& peerDomainName) const;

    /**
     * Get the endpoint that the given peer is listening on, or an std::nullopt if none is found
     */
    std::optional<MediatorEndpoint> lookupMediatorEndpoint(const std::string& peerDomainName) const;

private:
    void addClusterArgs(MediatorFunctionalTest* mediator);

private:
    std::vector<std::unique_ptr<MediatorFunctionalTest>> m_mediators;
    nx::cloud::discovery::test::DiscoveryServer m_discoveryServer;
};

} // namespace nx::hpm::test