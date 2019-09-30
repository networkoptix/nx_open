#include "mediator_endpoint.h"

#include <nx/fusion/model_functions.h>

namespace nx::hpm {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((MediatorEndpoint), (json), _Fields)

std::string MediatorEndpoint::toString() const
{
    return QJson::serialized(*this).toStdString();
}

bool MediatorEndpoint::operator==(const MediatorEndpoint& other) const
{
    return
        domainName == other.domainName
        && httpPort == other.httpPort
        && httpsPort == other.httpsPort
        && stunUdpPort == other.stunUdpPort;
}

bool MediatorEndpoint::operator !=(const MediatorEndpoint& other) const
{
    return !(*this == other);
}

} // namespace nx::hpm