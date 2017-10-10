#include "vms_gateway.h"

#include "../settings.h"

namespace nx {
namespace cdb {

VmsGateway::VmsGateway(const conf::Settings& settings):
    m_settings(settings)
{
}

void VmsGateway::merge(
    const std::string& /*targetSystemId*/,
    nx::utils::MoveOnlyFunc<void()> /*completionHandler*/)
{
    // TODO
}

} // namespace cdb
} // namespace nx
