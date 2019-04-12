#include "get_hardware_info_rest_handler.h"

#include <nx/network/http/http_types.h>
#include "licensing/hardware_info.h"
#include "llutil/hardware_id.h"

namespace nx::vms::server {

nx::network::rest::Response HardwareInfoHandler::executeGet(
    const nx::network::rest::Request& /*request*/)
{
    return nx::network::rest::Response::reply(LLUtil::getHardwareInfo());
}

} // namespace nx::vms::server
