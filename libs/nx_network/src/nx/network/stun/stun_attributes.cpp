// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "stun_attributes.h"

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace network {
namespace stun {
namespace attrs {

//-------------------------------------------------------------------------------------------------
// class AttributeFactory

std::unique_ptr<SerializableAttribute> AttributeFactory::create(int attributeType)
{
    switch (attributeType)
    {
        case attrs::mappedAddress:
            return std::make_unique<MappedAddress>();

        case attrs::alternateServer:
            return std::make_unique<AlternateServer>();

        default:
            return nullptr;
    }
}

//-------------------------------------------------------------------------------------------------
// class AddressAttribute

AddressAttribute::AddressAttribute(int type):
    m_type(type)
{
}

AddressAttribute::AddressAttribute(int type, SocketAddress endpoint):
    m_type(type),
    m_endpoint(std::move(endpoint))
{
    NX_ASSERT(m_endpoint.address.isIpAddress());
    NX_ASSERT(m_endpoint.address.ipV4() || m_endpoint.address.ipV6().first);
}

int AddressAttribute::getType() const
{
    return m_type;
}

nx::network::server::SerializerState AddressAttribute::serialize(
    MessageSerializerBuffer* buffer,
    std::size_t* bytesWritten) const
{
    const auto initialPosition = buffer->position();

    if (buffer->WriteByte(0) == nullptr)
        return nx::network::server::SerializerState::needMoreBufferSpace;

    const uint8_t addressFamily = m_endpoint.address.ipV4() ? kAddressFamilyIpV4 : kAddressFamilyIpV6;
    if (buffer->WriteByte(addressFamily) == nullptr)
        return nx::network::server::SerializerState::needMoreBufferSpace;

    if (buffer->WriteUint16(m_endpoint.port) == nullptr)
        return nx::network::server::SerializerState::needMoreBufferSpace;

    if (const auto ipv4Address = m_endpoint.address.ipV4())
    {
        if (buffer->WriteBytes(reinterpret_cast<const char*>(&ipv4Address->s_addr), 4) == nullptr)
            return nx::network::server::SerializerState::needMoreBufferSpace;
    }
    else if (const auto ipv6Address = m_endpoint.address.ipV6().first)
    {
        if (buffer->WriteBytes(reinterpret_cast<const char*>(&ipv6Address->s6_addr), 16) == nullptr)
            return nx::network::server::SerializerState::needMoreBufferSpace;
    }

    *bytesWritten = buffer->position() - initialPosition;

    return nx::network::server::SerializerState::done;
}

bool AddressAttribute::deserialize(MessageParserBuffer* buffer)
{
    bool ok = false;

    const uint8_t zeroByte = buffer->NextByte(&ok);
    if (!ok)
        return false;
    if (zeroByte != 0)
        return false;

    const uint8_t addressFamily = buffer->NextByte(&ok);
    if (!ok)
        return false;
    if (addressFamily != kAddressFamilyIpV4 && addressFamily != kAddressFamilyIpV6)
        return false;

    m_endpoint.port = buffer->NextUint16(&ok);
    if (!ok)
        return false;

    switch (addressFamily)
    {
        case kAddressFamilyIpV4:
        {
            in_addr addr;
            memset(&addr, 0, sizeof(addr));
            buffer->readNextBytesToBuffer(reinterpret_cast<char*>(&addr.s_addr), 4, &ok);
            if (!ok)
                return false;
            m_endpoint.address = HostAddress(addr);
            break;
        }

        case kAddressFamilyIpV6:
        {
            in6_addr addr;
            memset(&addr, 0, sizeof(addr));
            buffer->readNextBytesToBuffer(reinterpret_cast<char*>(&addr.s6_addr), 16, &ok);
            if (!ok)
                return false;
            m_endpoint.address = HostAddress(addr);
            break;
        }

        default:
            return false;
    }

    return true;
}

const SocketAddress& AddressAttribute::endpoint() const
{
    return m_endpoint;
}

const SocketAddress& AddressAttribute::get() const
{
    return m_endpoint;
}

bool AddressAttribute::operator==(const AddressAttribute& rhs) const
{
    return m_endpoint == rhs.m_endpoint;
}

//-------------------------------------------------------------------------------------------------
// class MappedAddress

MappedAddress::MappedAddress():
    AddressAttribute(TYPE)
{
}

MappedAddress::MappedAddress(SocketAddress endpoint):
    AddressAttribute(TYPE, std::move(endpoint))
{
}

//-------------------------------------------------------------------------------------------------
// class AlternateServer

AlternateServer::AlternateServer():
    AddressAttribute(TYPE)
{
}

AlternateServer::AlternateServer(SocketAddress endpoint):
    AddressAttribute(TYPE, std::move(endpoint))
{
}

//-------------------------------------------------------------------------------------------------
// class XorMappedAddress

static_assert(sizeof(in6_addr().s6_addr) == sizeof(XorMappedAddress().address.ipv6.words),
    "XorMappedAddress().address.ipv6 type is wrong");

XorMappedAddress::XorMappedAddress(int port_, uint32_t ipv4_):
    family(IPV4),
    port(port_)
{
    address.ipv4 = ipv4_;
}

XorMappedAddress::XorMappedAddress(int port_, Ipv6 ipv6_):
    family(IPV6),
    port(port_)
{
    address.ipv6 = ipv6_;
}

XorMappedAddress::XorMappedAddress(const SocketAddress& addr)
{
    port = addr.port;
    if (addr.address.ipV4())
    {
        family = kAddressFamilyIpV4;
        address.ipv4 = htonl(addr.address.ipV4()->s_addr);
    }
    else if (addr.address.ipV6().first)
    {
        family = kAddressFamilyIpV6;
        memcpy(address.ipv6.words, addr.address.ipV6().first->s6_addr,
            sizeof(addr.address.ipV6().first->s6_addr));
    }
}

SocketAddress XorMappedAddress::addr() const
{
    SocketAddress result;
    result.port = port;
    if (family == kAddressFamilyIpV4)
    {
        in_addr ipv4;
        memset(&ipv4, 0, sizeof(ipv4));
        ipv4.s_addr = ntohl(address.ipv4);
        result.address = HostAddress(ipv4);
    }
    else if (family == kAddressFamilyIpV6)
    {
        in6_addr ipv6;
        memset(&ipv6, 0, sizeof(ipv6));
        memcpy(ipv6.s6_addr, address.ipv6.words, sizeof(ipv6.s6_addr));
        result.address = HostAddress(ipv6);
    }

    return result;
}

//-------------------------------------------------------------------------------------------------
// class BufferedValue

BufferedValue::BufferedValue(nx::Buffer buffer_)
    : m_buffer(std::move(buffer_))
{
}

const Buffer& BufferedValue::getBuffer() const
{
    return m_buffer;
}

void BufferedValue::setBuffer(Buffer buf)
{
    m_buffer = std::move(buf);
}

std::string BufferedValue::getString() const
{
    return toStdString(m_buffer);
}

UserName::UserName(std::string value):
    BufferedValue(nx::Buffer(std::move(value)))
{
}

//-------------------------------------------------------------------------------------------------
// ErrorCode

ErrorCode::ErrorCode(int code_, std::string phrase):
    BufferedValue(nx::Buffer(phrase)),
    m_code(code_),
    m_reason(std::move(phrase))
{
}

//-------------------------------------------------------------------------------------------------
// FingerPrint

FingerPrint::FingerPrint(uint32_t crc32_):
    crc32(crc32_)
{
}


MessageIntegrity::MessageIntegrity(nx::Buffer hmac):
    BufferedValue(std::move(hmac))
{
}

Nonce::Nonce(Buffer nonce):
    BufferedValue(std::move(nonce))
{
}

Unknown::Unknown(int userType_, nx::Buffer value):
    BufferedValue(std::move(value)),
    userType(userType_)
{
}


//-------------------------------------------------------------------------------------------------
// IntAttribute

IntAttribute::IntAttribute(int userType, int value):
    Unknown(userType)
{
    const int valueInNetworkByteOrder = htonl(value);
    setBuffer(
        Buffer(
            reinterpret_cast<const char*>(&valueInNetworkByteOrder),
            sizeof(valueInNetworkByteOrder)));
}

int IntAttribute::value() const
{
    const auto& buf = getBuffer();
    int valueInNetworkByteOrder = 0;
    if (buf.size() != sizeof(valueInNetworkByteOrder))
        return 0;

    memcpy(
        &valueInNetworkByteOrder,
        buf.data(),
        sizeof(valueInNetworkByteOrder));
    return ntohl(valueInNetworkByteOrder);
}

//-------------------------------------------------------------------------------------------------
// Int64Attribute

Int64Attribute::Int64Attribute(int userType, std::int64_t value):
    Unknown(userType)
{
    const std::int64_t valueInNetworkByteOrder = htonll(value);
    setBuffer(
        Buffer(
            reinterpret_cast<const char*>(&valueInNetworkByteOrder),
            sizeof(valueInNetworkByteOrder)));
}

std::int64_t Int64Attribute::value() const
{
    const auto& buf = getBuffer();
    std::int64_t valueInNetworkByteOrder = 0;
    if (buf.size() != sizeof(valueInNetworkByteOrder))
        return 0;

    memcpy(
        &valueInNetworkByteOrder,
        buf.data(),
        sizeof(valueInNetworkByteOrder));

    return ntohll(valueInNetworkByteOrder);
}

//-------------------------------------------------------------------------------------------------
// Priority

Priority::Priority(int value):
    IntAttribute(TYPE, value)
{
}

//-------------------------------------------------------------------------------------------------
// Lifetime

Lifetime::Lifetime(int value):
    IntAttribute(TYPE, value)
{
}

//-------------------------------------------------------------------------------------------------
// RequestedTransport

RequestedTransport::RequestedTransport(Transport transport):
    Unknown(TYPE)
{
    constexpr int kRequestedTransportValueSize = 4;
    char value[] = {(char)transport, 0, 0, 0};
    setBuffer({value, kRequestedTransportValueSize});
}

} // namespace attrs
} // namespace stun
} // namespace network
} // namespace nx
