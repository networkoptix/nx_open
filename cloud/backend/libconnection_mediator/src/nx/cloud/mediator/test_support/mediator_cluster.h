#pragma once

#include <nx/cloud/mediator/mediator_service.h>
#include <nx/cloud/mediator/listening_peer_db.h>
#include <nx/cloud/mediator/test_support/mediator_functional_test.h>
#include <nx/cloud/discovery/test_support/discovery_server.h>

namespace nx::hpm::test {

using Mediator = MediatorFunctionalTest;

/**
 * Provides support for mediators running in "cluster" mode.
 */
class MediatorCluster
{
public:
    /**
     * Throws runtime_error if internal discovery server fails to initialize.
     */
    MediatorCluster();

    /**
     * Adds a mediator to the cluster with necessary arguments for cluster operation.
     * NOTE: Does NOT call startAndWaitUntilStarted() on the mediator to allow further processing
     * by the user.
     */
    Mediator& addMediator(
        std::vector <const char*> args = {},
        Mediator::MediatorTestFlags = Mediator::MediatorTestFlags::allFlags);

    /*
     * Get the number of nodes (mediators) in the cluster
     */
    int size() const;

    Mediator& mediator(int index);
    const Mediator& mediator(int index) const;

    /**
     * returns true if ALL Mediator's have peerDomainName.
     */
    bool peerInformationSynchronizedInCluster(const std::string& peerDomainName) const;

    /**
     * returns true if ALL Mediator's do NOT have peerDomainName.
     */
    bool peerInformationIsAbsentFromCluster(const std::string& peerDomainName) const;

private:
    std::vector<std::unique_ptr<Mediator>> m_mediators;
    nx::cloud::discovery::test::DiscoveryServer m_discoveryServer;
};

} // namespace nx::hpm::test