#include "mediator_process_public.h"

#include "mediator_service.h"

namespace nx {
namespace hpm {

MediatorProcessPublic::MediatorProcessPublic(int argc, char **argv):
    m_impl(new MediatorProcess(argc, argv))
{
}

MediatorProcessPublic::~MediatorProcessPublic()
{
    delete m_impl;
    m_impl = nullptr;
}

void MediatorProcessPublic::setOnStartedEventHandler(
    nx::utils::MoveOnlyFunc<void(bool)> handler)
{
    m_impl->setOnStartedEventHandler(std::move(handler));
}

void MediatorProcessPublic::pleaseStop()
{
    m_impl->pleaseStop();
}

int MediatorProcessPublic::exec()
{
    return m_impl->exec();
}

const MediatorProcess* MediatorProcessPublic::impl() const
{
    return m_impl;
}

} // namespace hpm
} // namespace nx
