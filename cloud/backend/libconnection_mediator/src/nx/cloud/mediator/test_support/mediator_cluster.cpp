#include "mediator_cluster.h"

namespace nx::hpm::test {

Mediator& MediatorCluster::addMediator(
    std::vector<const char*> args,
    Mediator::MediatorTestFlags flags)
{
    auto& mediator = m_mediators.emplace_back(std::make_unique<Mediator>(flags));
    for (const char* arg : args)
        mediator->addArg(arg);

    return *mediator;
}

Mediator& MediatorCluster::mediator(int index)
{
    return *m_mediators[index];
}

const Mediator& MediatorCluster::mediator(int index) const
{
    return *m_mediators[index];
}

} // namespace nx::hpm::test