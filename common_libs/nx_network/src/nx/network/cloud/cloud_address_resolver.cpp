#include "cloud_address_resolver.h"

#include <nx/network/socket_global.h>

#include "cloud_connect_controller.h"
#include "mediator_connector.h"

namespace nx {
namespace network {

CloudAddressResolver::CloudAddressResolver():
    m_cloudAddressRegExp(QLatin1String(
        "(.+\\.)?[0-9a-f]{8}\\-[0-9a-f]{4}\\-[0-9a-f]{4}\\-[0-9a-f]{4}\\-[0-9a-f]{12}"))
{
}

SystemError::ErrorCode CloudAddressResolver::resolve(
    const QString& hostName,
    int /*ipVersion*/,
    std::deque<AddressEntry>* resolvedAddresses)
{
    QnMutexLocker locker(&m_mutex);

    if (!m_cloudAddressRegExp.exactMatch(hostName))
        return SystemError::hostNotFound;

    resolvedAddresses->push_back(AddressEntry(AddressType::cloud, HostAddress(hostName)));
    return SystemError::noError;
}

} // namespace network
} // namespace nx
