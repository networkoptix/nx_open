#pragma once

#include "../socket_common.h"

namespace nx {
namespace network {

enum class AddressType
{
    unknown,
    direct, /**< Address for direct simple connection. */
    cloud, /**< Address that requires mediator (e.g. hole punching). */
};

QString toString(const AddressType& type);

struct TypedAddress
{
    HostAddress address;
    AddressType type;

    TypedAddress(HostAddress address_, AddressType type_);
    QString toString() const;
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
    QString toString() const;
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
    QString toString() const;
    SocketAddress toEndpoint() const;
};

} // namespace network
} // namespace nx
