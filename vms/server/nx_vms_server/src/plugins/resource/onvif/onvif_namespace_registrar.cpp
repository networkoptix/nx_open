#include "onvif_namespace_registrar.h"

#include <map>

#include "soap_wrapper.h"

namespace nx {
namespace vms::server {
namespace plugins {
namespace onvif {

#define COMMON_NAMESPACES \
    {"SOAP-ENV", "http://www.w3.org/2003/05/soap-envelope", nullptr, nullptr},\
    {"SOAP-ENC", "http://www.w3.org/2003/05/soap-encoding", nullptr, nullptr},\
    {"xsi", "http://www.w3.org/2001/XMLSchema-instance", nullptr, nullptr},\
    {"xsd", "http://www.w3.org/2001/XMLSchema", nullptr, nullptr},\
    {"wsa5", "http://www.w3.org/2005/08/addressing", nullptr, nullptr}, \
    {\
        "wsse",\
        "http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd",\
        nullptr,\
        nullptr\
    },\
    {\
        "wsu",\
        "http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd",\
        nullptr,\
        nullptr\
    },\
    {\
        "onvifXsd",\
        "http://www.onvif.org/ver10/schema",\
        nullptr,\
        nullptr\
    }
//    {nullptr, nullptr, nullptr, nullptr}

namespace {

template<typename Request>
RequestTypeId typeId()
{
    return Request().soap_type();
}

static const std::map<RequestTypeId, std::vector<Namespace>> kOverridenNamespaces = {
    {
        typeId<_onvifPtz__AbsoluteMove>(),
        {
            COMMON_NAMESPACES,
            {"onvifPtz", "http://www.onvif.org/ver20/ptz/wsdl", nullptr, nullptr},
            { nullptr, nullptr, nullptr, nullptr }
        }
    },
    {
        typeId<_onvifMedia__GetVideoSources>(),
        {
            COMMON_NAMESPACES,
            {"onvifMedia", "http://www.onvif.org/ver10/media/wsdl", nullptr, nullptr},
            { nullptr, nullptr, nullptr, nullptr }
        }
    },
    {
        typeId<_onvifEvents__PullMessages>(),
        {
            COMMON_NAMESPACES,
            {"onvifEvents", "http://www.onvif.org/ver10/events/wsdl", nullptr, nullptr},
            { nullptr, nullptr, nullptr, nullptr }
        }
    }
};

} // namespace

const Namespace* requestNamespaces(RequestTypeId typeId)
{
    const auto itr = kOverridenNamespaces.find(typeId);
    if (itr == kOverridenNamespaces.cend())
        return nullptr;

    return itr->second.data();
}

} // namespaec onvif
} // namespace plugins
} // namespace vms::server
} // namespace nx
