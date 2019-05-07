#pragma once

#include <nx/cloud/mediator/mediator_service.h>
#include <nx/cloud/mediator/listening_peer_db.h>
#include <nx/cloud/mediator/test_support/mediator_functional_test.h>
#include <nx/cloud/discovery/test_support/discovery_server.h>

namespace nx::hpm::test {
/**
 * Provides support for mediators running in "cluster" mode.
 */
class MediatorCluster: public nx::utils::test::TestWithTemporaryDirectory
{
    using base_type = nx::utils::test::TestWithTemporaryDirectory;
public:
    MediatorCluster(const nx::utils::Url& discoveryServiceUrl);
    ~MediatorCluster();

    /**
     * Stops all mediators.
     */
    void stop();

    /**
     * Adds a mediator to the cluster with necessary arguments for cluster operation.
     * NOTE: Does NOT call startAndWaitUntilStarted() on the mediator to allow further processing
     * by the user.
     */
    MediatorInstance& addMediator(
        int flags = MediatorInstance::MediatorTestFlags::allFlags,
        QString testDir = QString());

    /**
     * Adds a mediator to the cluster with necessary arguments for cluster operation.
     * NOTE: Does NOT call startAndWaitUntilStarted() on the mediator to allow further processing
     * by the user.
     */
    MediatorInstance& addMediator(
        std::vector <const char*> args,
        int flags = MediatorInstance::MediatorTestFlags::allFlags,
        QString testDir = QString());

    AbstractCloudDataProvider::System addRandomSystem();

    /*
     * Get the number of nodes (mediators) in the cluster
     */
    int size() const;

    MediatorInstance& mediator(int index);
    const MediatorInstance& mediator(int index) const;

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
    void addClusterArgs(int index, MediatorInstance* mediator);

    void registerCloudDataProvider(AbstractCloudDataProvider* cloudDataProvider);

private:
    std::vector<std::unique_ptr<MediatorInstance>> m_mediators;
    nx::utils::Url m_discoveryServiceUrl;

    boost::optional<AbstractCloudDataProviderFactory::FactoryFunc> m_factoryFuncToRestore;
    LocalCloudDataProvider m_cloudDataProvider;
};

} // namespace nx::hpm::test