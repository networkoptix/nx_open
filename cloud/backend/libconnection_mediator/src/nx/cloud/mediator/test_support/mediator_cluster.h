#pragma once

#include "mediator_functional_test.h"

namespace nx::hpm::test {

using Mediator = MediatorFunctionalTest;

class MediatorCluster
{
public:
    Mediator& addMediator(
        std::vector <const char*> args = {},
        Mediator::MediatorTestFlags = Mediator::MediatorTestFlags::allFlags);

    Mediator& mediator(int index);
    const Mediator& mediator(int index) const;

private:
    std::vector<std::unique_ptr<Mediator>> m_mediators;
};

} // namespace nx::hpm::test