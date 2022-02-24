// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "messages.h"

#include <nx/network/socket_common.h>

#include <nx/utils/log/log.h>

static_assert(sizeof(in_addr) == 4, "Incorrect system size of IPv4");
static_assert(sizeof(in6_addr) == 16, "Incorrect system size of IPv6");

namespace nx::network::socks5 {

namespace {

static constexpr uint8_t kSocks5Version = 0x05;
static constexpr uint8_t kAuthVersion = 0x01;

static constexpr size_t kGreetHeaderSize = 2;
static constexpr size_t kAuthHeaderSize = 2;
static constexpr size_t kConnectHeaderSize = 4;

static constexpr uint8_t kReservedNull = 0x00;

static constexpr uint8_t kAddressIPv4 = 0x01;
static constexpr uint8_t kAddressDomain = 0x03;
static constexpr uint8_t kAddressIPv6 = 0x04;

// Read 2-byte port value at offset.
uint16_t readPortAt(const nx::Buffer& buffer, size_t offset)
{
    NX_ASSERT(buffer.size() >= offset + sizeof(uint16_t));

    // Port is encoded with network byte order (big-endian).
    const uint8_t portHigh = buffer.at(offset);
    const uint8_t portLow = buffer.at(offset + 1);
    return ((uint16_t) portHigh << 8) | portLow;
}

// Read lenght-byte string starting at offset.
std::string readStringAt(const nx::Buffer& buffer, size_t offset, uint8_t length)
{
    NX_ASSERT(buffer.size() >= offset + length);

    return buffer.substr(offset, length).toString();
}

// Append string length (1 byte) + string itself to buffer.
void appendString(nx::Buffer* buffer, const std::string& str)
{
    NX_ASSERT(str.size() > 0);
    NX_ASSERT(str.size() <= std::numeric_limits<uint8_t>::max());

    buffer->append((char) str.size());
    buffer->append(str);
}

// Append 2-byte port value to buffer.
void appendPort(nx::Buffer* buffer, uint16_t port)
{
    // Port is encoded with network byte order (big-endian).
    buffer->append((char) ((port & 0xFF00) >> 8));
    buffer->append((char) (port & 0xFF));
}

// Parse host and port from connect request buffer.
Message::ParseResult parseAddress(const nx::Buffer& buffer, std::string* host, uint16_t* port)
{
    const int addressType = buffer.at(3);
    switch (addressType)
    {
        case kAddressIPv4:
        {
            const auto completeLength = kConnectHeaderSize + sizeof(in_addr) + sizeof(uint16_t);

            if (buffer.size() < completeLength)
                return Message::ParseStatus::NeedMoreData;

            const std::optional<std::string> ipv4String = HostAddress::ipToString(
                *reinterpret_cast<const in_addr*>(&buffer[kConnectHeaderSize]));

            if (!ipv4String)
            {
                NX_DEBUG(NX_SCOPE_TAG, "Invalid IPv4 data");
                return Message::ParseStatus::Error;
            }

            *host = *ipv4String;
            *port = readPortAt(buffer, kConnectHeaderSize + sizeof(in_addr));
            return { Message::ParseStatus::Complete, completeLength };
        }
        case kAddressIPv6:
        {
            const auto completeLength = kConnectHeaderSize + sizeof(in6_addr) + sizeof(uint16_t);

            if (buffer.size() < completeLength)
                return Message::ParseStatus::NeedMoreData;

            const std::optional<std::string> ipv6String = HostAddress::ipToString(
                *reinterpret_cast<const in6_addr*>(&buffer[kConnectHeaderSize]), std::nullopt);

            if (!ipv6String)
            {
                NX_DEBUG(NX_SCOPE_TAG, "Invalid IPv6 data");
                return Message::ParseStatus::Error;
            }

            *host = *ipv6String;
            *port = readPortAt(buffer, kConnectHeaderSize + sizeof(in6_addr));
            return { Message::ParseStatus::Complete, completeLength };
        }
        case kAddressDomain:
        {
            // Need domain length byte to determine complete message length.
            if (buffer.size() < kConnectHeaderSize + 1)
                return Message::ParseStatus::NeedMoreData;

            const uint8_t length = buffer.at(kConnectHeaderSize);
            if (length == 0)
            {
                NX_DEBUG(NX_SCOPE_TAG, "Empty domain name");
                return Message::ParseStatus::Error;
            }

            const auto completeLength = kConnectHeaderSize + 1 + length + sizeof(uint16_t);

            if (buffer.size() < completeLength)
                return Message::ParseStatus::NeedMoreData;

            *host = readStringAt(buffer, kConnectHeaderSize + 1, length);
            *port = readPortAt(buffer, kConnectHeaderSize + 1 + length);
            return { Message::ParseStatus::Complete, completeLength };
        }
        default:
            NX_DEBUG(NX_SCOPE_TAG, "Unknown SOCKS5 address type: %1", addressType);
            return Message::ParseStatus::Error;
    }
}

} // namespace

Message::ParseResult GreetRequest::parse(const nx::Buffer& buffer)
{
    // Need a full header to determine exact length of the greet message.
    if (buffer.size() < kGreetHeaderSize)
        return Message::ParseStatus::NeedMoreData;

    if (buffer.at(0) != kSocks5Version)
    {
        NX_DEBUG(this, "Client requested SOCKS version %1", buffer.at(0));
        return Message::ParseStatus::Error;
    }

    // Now we can figure out the length of the complete greet message.
    const uint8_t methodCount = buffer.at(1);
    if (methodCount == 0)
    {
        NX_DEBUG(this, "Client supports no authentication methods");
        return Message::ParseStatus::Error;
    }

    const auto completeLength = kGreetHeaderSize + methodCount;

    if (buffer.size() < completeLength)
        return Message::ParseStatus::NeedMoreData;

    // Copy all methods from buffer.
    methods.resize(methodCount);
    memmove(methods.data(), &buffer[kGreetHeaderSize], methodCount);
    return { Message::ParseStatus::Complete, completeLength };
}

nx::Buffer GreetRequest::toBuffer() const
{
    NX_ASSERT(methods.size() > 0);
    NX_ASSERT(methods.size() <= std::numeric_limits<uint8_t>::max());

    nx::Buffer buffer(kGreetHeaderSize + methods.size(), 0);
    buffer[0] = kSocks5Version;
    buffer[1] = methods.size();
    memmove(&buffer[kGreetHeaderSize], methods.data(), methods.size());
    return buffer;
}

Message::ParseResult GreetResponse::parse(const nx::Buffer& buffer)
{
    if (buffer.size() < kGreetHeaderSize)
        return Message::ParseStatus::NeedMoreData;

    if (buffer.at(0) != kSocks5Version)
    {
        NX_DEBUG(this, "Client requested SOCKS version %1", buffer.at(0));
        return Message::ParseStatus::Error;
    }

    method = buffer.at(1);

    return { Message::ParseStatus::Complete, kGreetHeaderSize };
}

nx::Buffer GreetResponse::toBuffer() const
{
    const uint8_t response[2] = { kSocks5Version, method };
    return nx::Buffer((char*) &response, sizeof(response));
}

Message::ParseResult AuthRequest::parse(const nx::Buffer& buffer)
{
    // First we need a full header to determine length of username.
    if (buffer.size() < kAuthHeaderSize)
        return Message::ParseStatus::NeedMoreData;

    if (buffer.at(0) != kAuthVersion)
    {
        NX_DEBUG(this, "Client requested SOCKS auth version %1", buffer.at(0));
        return Message::ParseStatus::Error;
    }

    // Need to fully read username and a byte with password length to get complete message length.
    const uint8_t userLength = buffer.at(1);
    if (userLength == 0)
    {
        NX_DEBUG(this, "Client sent empty user name");
        return Message::ParseStatus::Error;
    }

    if (buffer.size() < kAuthHeaderSize + userLength + 1)
        return Message::ParseStatus::NeedMoreData;

    const uint8_t passLength = buffer.at(kAuthHeaderSize + userLength);
    if (passLength == 0)
    {
        NX_DEBUG(this, "Client sent empty password");
        return Message::ParseStatus::Error;
    }

    const auto completeLength = kAuthHeaderSize + userLength + 1 + passLength;

    if (buffer.size() < completeLength)
        return Message::ParseStatus::NeedMoreData;

    // Get the data from buffer.
    username = readStringAt(buffer, kAuthHeaderSize, userLength);
    password = readStringAt(buffer, kAuthHeaderSize + userLength + 1, passLength);

    return { Message::ParseStatus::Complete, completeLength };
}

nx::Buffer AuthRequest::toBuffer() const
{
    nx::Buffer buffer;

    buffer.append((char) kAuthVersion);

    appendString(&buffer, username);
    appendString(&buffer, password);

    return buffer;
}

Message::ParseResult AuthResponse::parse(const nx::Buffer& buffer)
{
    if (buffer.size() < kAuthHeaderSize)
        return Message::ParseStatus::NeedMoreData;

    if (buffer.at(0) != kAuthVersion)
    {
        NX_DEBUG(this, "Client requested SOCKS auth version %1", buffer.at(0));
        return Message::ParseStatus::Error;
    }

    status = buffer.at(1);

    return { Message::ParseStatus::Complete, kAuthHeaderSize };
}

nx::Buffer AuthResponse::toBuffer() const
{
    const uint8_t response[2] = { kAuthVersion, status };
    return nx::Buffer((char*) &response, sizeof(response));
}

Message::ParseResult ConnectRequest::parse(const nx::Buffer& buffer)
{
    // Destination host address can be encoded as either domain name or IPv4 or IPv6.
    // Determine which address type the client used and wait for the complete message.

    if (buffer.size() < kConnectHeaderSize)
        return Message::ParseStatus::NeedMoreData;

    // Verify header content.
    if (buffer.at(0) != kSocks5Version)
    {
        NX_DEBUG(this, "Client requested SOCKS version %1", buffer.at(0));
        return Message::ParseStatus::Error;
    }

    if (buffer.at(2) != kReservedNull)
    {
        NX_DEBUG(this, "Invalid reserved byte in header: %1", buffer.at(2));
        return Message::ParseStatus::Error;
    }

    command = buffer.at(1);

    // Got the header, now get address type and try to read the remaining message. If the message
    // is not complete just return and wait for the remaining parts.
    return parseAddress(buffer, &host, &port);
}

nx::Buffer ConnectRequest::toBuffer() const
{
    const uint8_t request[4] = {
        kSocks5Version,
        command,
        kReservedNull,
        kAddressDomain};

    nx::Buffer buffer((char*) &request, sizeof(request));

    appendString(&buffer, host);
    appendPort(&buffer, port);

    return buffer;
}

Message::ParseResult ConnectResponse::parse(const nx::Buffer& buffer)
{
    // Destination host address can be encoded as either domain name or IPv4 or IPv6.
    // Determine which address type the client used and wait for the complete message.

    if (buffer.size() < kConnectHeaderSize)
        return Message::ParseStatus::NeedMoreData;

    // Verify header content.
    if (buffer.at(0) != kSocks5Version)
    {
        NX_DEBUG(this, "Client requested SOCKS version %1", buffer.at(0));
        return Message::ParseStatus::Error;
    }

    if (buffer.at(2) != kReservedNull)
    {
        NX_DEBUG(this, "Invalid reserved byte in header: %1", buffer.at(2));
        return Message::ParseStatus::Error;
    }

    status = buffer.at(1);

    // We are generally not interested in response address data for TCP connection, but we still
    // need to report how many bytes have been parsed.
    return parseAddress(buffer, &host, &port);
}

nx::Buffer ConnectResponse::toBuffer() const
{
    const uint8_t response[4] = {
        kSocks5Version,
        status,
        kReservedNull,
        kAddressDomain};

    nx::Buffer buffer((char*) &response, sizeof(response));

    // Always encode host as domain name always.
    appendString(&buffer, host);
    appendPort(&buffer, port);

    return buffer;
}

} // namespace nx::network::socks5
