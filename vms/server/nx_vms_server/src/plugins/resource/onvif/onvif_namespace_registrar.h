#pragma once

#include <onvif/soapStub.h>

namespace nx {
namespace vms::server {
namespace plugins {
namespace onvif {

using RequestTypeId = int;

const Namespace* requestNamespaces(RequestTypeId typeId);

template<typename Request>
const Namespace* requestNamespaces()
{
    return requestNamespaces(Request().soap_type());
}

} // namespace onvif
} // namespace plugins
} // namespace vms::server
} // namespace nx
