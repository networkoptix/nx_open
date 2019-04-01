#pragma once

#include <nx/cloud/mediator/mediator_service.h>
#include <nx/cloud/mediator/test_support/mediator_functional_test.h>

#include <nx/cloud/mediator/remote_mediator_peer_pool.h>

namespace nx::hpm::test {

using Mediator = MediatorFunctionalTest;

class MediatorCluster
{
public:
    Mediator& addMediator(
        std::vector <const char*> args = {},
        Mediator::MediatorTestFlags = Mediator::MediatorTestFlags::allFlags);

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
};

} // namespace nx::hpm::test