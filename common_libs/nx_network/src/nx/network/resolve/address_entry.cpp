#include "address_entry.h"

#include <nx/utils/log/log.h>

namespace nx {
namespace network {

QString toString(const AddressType& type)
{
    switch (type)
    {
        case AddressType::unknown: return "unknown";
        case AddressType::direct: return "direct";
        case AddressType::cloud: return "cloud";
    };

    NX_ASSERT(false, Q_FUNC_INFO, "undefined AddressType");
    return lm("undefined=%1").arg(static_cast<int>(type));
}

//-------------------------------------------------------------------------------------------------

TypedAddress::TypedAddress(HostAddress address_, AddressType type_):
    address(std::move(address_)),
    type(std::move(type_))
{
}

QString TypedAddress::toString() const
{
    return lm("%1(%2)").args(address, type);
}

//-------------------------------------------------------------------------------------------------

AddressAttribute::AddressAttribute(AddressAttributeType type_, quint64 value_):
    type(type_),
    value(value_)
{
}

bool AddressAttribute::operator==(const AddressAttribute& rhs) const
{
    return type == rhs.type && value == rhs.value;
}

bool AddressAttribute::operator <(const AddressAttribute& rhs) const
{
    return type < rhs.type && value < rhs.value;
}

QString AddressAttribute::toString() const
{
    switch (type)
    {
        case AddressAttributeType::unknown:
            return "unknown";
        case AddressAttributeType::port:
            return lm("port=%1").arg(value);
    };

    NX_ASSERT(false, Q_FUNC_INFO, "undefined AddressAttributeType");
    return lm("undefined=%1").arg(static_cast<int>(type));
}

//-------------------------------------------------------------------------------------------------

AddressEntry::AddressEntry(AddressType type_, HostAddress host_):
    type(type_),
    host(std::move(host_))
{
}

AddressEntry::AddressEntry(const SocketAddress& address):
    type(AddressType::direct),
    host(address.address)
{
    if (address.port != 0)
    {
        attributes.push_back(AddressAttribute(
            AddressAttributeType::port, address.port));
    }
}

bool AddressEntry::operator==(const AddressEntry& rhs) const
{
    return type == rhs.type && host == rhs.host && attributes == rhs.attributes;
}

bool AddressEntry::operator<(const AddressEntry& rhs) const
{
    return type < rhs.type && host < rhs.host && attributes < rhs.attributes;
}

QString AddressEntry::toString() const
{
    return lm("%1:%2(%3)").arg(type).arg(host).container(attributes);
}

SocketAddress AddressEntry::toEndpoint() const
{
    SocketAddress endpoint;
    endpoint.address = host;

    auto portAttributeIter = std::find_if(
        attributes.begin(), attributes.end(),
        [](const AddressAttribute& attr) { return attr.type == AddressAttributeType::port; });
    if (portAttributeIter != attributes.end())
        endpoint.port = portAttributeIter->value;

    return endpoint;
}

} // namespace network
} // namespace nx
