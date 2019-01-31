/**********************************************************
* May 17, 2016
* akolesnikov
***********************************************************/

#include "vms_gateway_process_public.h"

#include "vms_gateway_process.h"


namespace nx {
namespace cloud {
namespace gateway {

VmsGatewayProcessPublic::VmsGatewayProcessPublic(int argc, char **argv)
:
    m_impl(new VmsGatewayProcess(argc, argv))
{
}

VmsGatewayProcessPublic::~VmsGatewayProcessPublic()
{
    delete m_impl;
    m_impl = nullptr;
}

void VmsGatewayProcessPublic::pleaseStop()
{
    m_impl->pleaseStop();
}

void VmsGatewayProcessPublic::setOnStartedEventHandler(
    nx::utils::MoveOnlyFunc<void(bool)> handler)
{
    m_impl->setOnStartedEventHandler(std::move(handler));
}

int VmsGatewayProcessPublic::exec()
{
    return m_impl->exec();
}

VmsGatewayProcess* VmsGatewayProcessPublic::impl() const
{
    return m_impl;
}

}   //namespace gateway
}   //namespace cloud
}   //namespace nx
