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
// class MappedAddress

MappedAddress::MappedAddress()
{
}

MappedAddress::MappedAddress(SocketAddress endpoint):
    m_endpoint(std::move(endpoint))
{
    NX_ASSERT(m_endpoint.address.isIpAddress());
    NX_ASSERT(m_endpoint.address.ipV4() || m_endpoint.address.ipV6().first);
}

int MappedAddress::getType() const
{
    return TYPE;
}

nx::network::server::SerializerState MappedAddress::serialize(
    MessageSerializerBuffer* buffer,
    std::size_t* bytesWritten) const
{
    const auto initialPosition = buffer->position();

    if (buffer->WriteByte(0) == nullptr)
        return nx::network::server::SerializerState::needMoreBufferSpace;

    const uint8_t addressFamily = m_endpoint.address.ipV4() ? kAddressTypeIpV4 : kAddressTypeIpV6;
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

bool MappedAddress::deserialize(MessageParserBuffer* buffer)
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
    if (addressFamily != kAddressTypeIpV4 && addressFamily != kAddressTypeIpV6)
        return false;

    m_endpoint.port = buffer->NextUint16(&ok);
    if (!ok)
        return false;

    switch (addressFamily)
    {
        case kAddressTypeIpV4:
        {
            in_addr addr;
            memset(&addr, 0, sizeof(addr));
            buffer->readNextBytesToBuffer(reinterpret_cast<char*>(&addr.s_addr), 4, &ok);
            if (!ok)
                return false;
            m_endpoint.address = HostAddress(addr);
            break;
        }

        case kAddressTypeIpV6:
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

const SocketAddress& MappedAddress::endpoint() const
{
    return m_endpoint;
}

bool MappedAddress::operator==(const MappedAddress& rhs) const
{
    return m_endpoint == rhs.m_endpoint;
}

//-------------------------------------------------------------------------------------------------
// class AlternateServer

AlternateServer::AlternateServer()
{
}

AlternateServer::AlternateServer(SocketAddress endpoint):
    MappedAddress(std::move(endpoint))
{
}

int AlternateServer::getType() const
{
    return TYPE;
}

//-------------------------------------------------------------------------------------------------
// class XorMappedAddress

XorMappedAddress::XorMappedAddress():
    family(0), // invalid
    port(0)
{
}

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

String BufferedValue::getString() const
{
    return bufferToString(m_buffer);
}

UserName::UserName(const String& value):
    BufferedValue(stringToBuffer(value))
{
}

//-------------------------------------------------------------------------------------------------
// ErrorCode

ErrorCode::ErrorCode(int code_, const nx::String& phrase):
    BufferedValue(bufferToString(phrase)),
    m_code(code_),
    m_reason(bufferToString(phrase))
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


////////////////////////////////////////////////////////////
//// IntAttribute
////////////////////////////////////////////////////////////

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
        buf.constData(),
        sizeof(valueInNetworkByteOrder));
    return ntohl(valueInNetworkByteOrder);
}

} // namespace attrs
} // namespace stun
} // namespace network
} // namespace nx
