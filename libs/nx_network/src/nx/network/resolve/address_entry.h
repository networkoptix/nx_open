// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <ostream>

#include "../socket_common.h"

namespace nx::network {

enum class AddressType
{
    unknown,
    direct, /**< Address for direct simple connection. */
    cloud, /**< Address that requires mediator (e.g. hole punching). */
};

std::string toString(const AddressType& type);

struct TypedAddress
{
    HostAddress address;
    AddressType type;

    TypedAddress(HostAddress address_, AddressType type_);
    std::string toString() const;
};

enum class AddressAttributeType
{
    unknown,
    /** NX peer port. */
    port,
};

struct NX_NETWORK_API AddressAttribute
{
    AddressAttributeType type;
    quint64 value; // TODO: boost::variant when int is not enough

    AddressAttribute(AddressAttributeType type_, quint64 value_ = 0);
    bool operator ==(const AddressAttribute& rhs) const;
    bool operator <(const AddressAttribute& rhs) const;
    std::string toString() const;
};

struct NX_NETWORK_API AddressEntry
{
    AddressType type;
    HostAddress host;
    std::vector<AddressAttribute> attributes;

    AddressEntry(
        AddressType type_ = AddressType::unknown,
        HostAddress host_ = HostAddress());

    AddressEntry(const SocketAddress& address);

    bool operator ==(const AddressEntry& rhs) const;
    bool operator <(const AddressEntry& rhs) const;
    std::string toString() const;
    SocketAddress toEndpoint() const;
};

inline std::ostream& operator<<(std::ostream& os, const AddressEntry& value)
{
    return os << value.toString();
}

} // namespace nx::network
