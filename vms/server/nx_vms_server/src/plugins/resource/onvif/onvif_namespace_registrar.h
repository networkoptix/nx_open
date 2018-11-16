#pragma once

#include <onvif/soapStub.h>

namespace nx {
namespace mediaserver_core {
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
} // namespace mediaserver_core
} // namespace nx
