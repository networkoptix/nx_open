// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "address_entry.h"

#include <nx/utils/log/log.h>
#include <nx/utils/string.h>

namespace nx::network {

std::string toString(const AddressType& type)
{
    switch (type)
    {
        case AddressType::unknown: return "unknown";
        case AddressType::direct: return "direct";
        case AddressType::cloud: return "cloud";
    };

    NX_ASSERT(false, "undefined AddressType");
    return nx::format("undefined=%1").arg(static_cast<int>(type)).toStdString();
}

//-------------------------------------------------------------------------------------------------

TypedAddress::TypedAddress(HostAddress address_, AddressType type_):
    address(std::move(address_)),
    type(std::move(type_))
{
}

std::string TypedAddress::toString() const
{
    return nx::utils::buildString(address.toString(), '(', network::toString(type), ')');
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

std::string AddressAttribute::toString() const
{
    switch (type)
    {
        case AddressAttributeType::unknown:
            return "unknown";
        case AddressAttributeType::port:
            return nx::format("port=%1").arg(value).toStdString();
    };

    NX_ASSERT(false, "undefined AddressAttributeType");
    return nx::format("undefined=%1").arg(static_cast<int>(type)).toStdString();
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

std::string AddressEntry::toString() const
{
    return nx::format("%1:%2(%3)").arg(network::toString(type)).arg(host).container(attributes)
        .toStdString();
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

} // namespace nx::network
