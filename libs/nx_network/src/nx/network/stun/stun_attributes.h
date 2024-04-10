// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <string>

#include <nx/network/connection_server/base_protocol_message_types.h>
#include <nx/network/socket_common.h>

#include "stun_message_parser_buffer.h"
#include "stun_message_serializer_buffer.h"

namespace nx {
namespace network {
namespace stun {

namespace error {

enum Value
{
    tryAlternate = 300,
    badRequest = 400,
    unauthtorized = 401,
    notFound = 404,
    unknownAttribute = 420,
    staleNonce = 438,
    serverError = 500,
};

} // namespace error

//!Contains STUN attributes
namespace attrs {

enum Type
{
    mappedAddress = 0x0001,
    reserved2 = 0x0002,
    reserved3 = 0x0003,
    reserved4 = 0x0004,
    reserved5 = 0x0005,
    userName = 0x0006,
    messageIntegrity = 0x0008,
    errorCode = 0x0009,
    unknown = 0x000A,
    lifetime = 0x000D,
    xorPeerAddress = 0x0012,
    data = 0x0013,
    realm = 0x0014,
    nonce = 0x0015,
    xorRelayedAddress = 0x0016,
    requestedTransport = 0x0019,
    accessToken = 0x001B,
    xorMappedAddress = 0x0020,
    priority = 0x0024,
    useCandidate = 0x0025,

    software = 0x8022,
    alternateServer = 0x8023,
    fingerPrint = 0x8028,
    iceControlled = 0x8029,
    iceControlling = 0x802A,
    thirdPartyAuthorization = 0x802E,

    // Starting value for custom NX attributes in the middle of unassigned range 0xC003-0xFFFF:
    // https://www.iana.org/assignments/stun-parameters/stun-parameters.txt
    userDefined = 0xE000,
};

struct NX_NETWORK_API Attribute
{
    virtual ~Attribute() {}

    virtual int getType() const = 0;
};

class NX_NETWORK_API SerializableAttribute:
    public Attribute
{
public:
    virtual nx::network::server::SerializerState serialize(
        MessageSerializerBuffer* buffer,
        std::size_t* bytesWritten) const = 0;

    virtual bool deserialize(MessageParserBuffer* buffer) = 0;
};

class AttributeFactory
{
public:
    static std::unique_ptr<SerializableAttribute> create(int attributeType);
};

static constexpr int kAddressFamilyIpV4 = 1;
static constexpr int kAddressFamilyIpV6 = 2;

class NX_NETWORK_API AddressAttribute:
    public SerializableAttribute
{
public:
    AddressAttribute(int type);
    AddressAttribute(int type, SocketAddress endpoint);

    virtual int getType() const override;

    virtual nx::network::server::SerializerState serialize(
        MessageSerializerBuffer* buffer,
        std::size_t* bytesWritten) const override;

    virtual bool deserialize(MessageParserBuffer* buffer) override;

    const SocketAddress& endpoint() const;
    const SocketAddress& get() const;

    bool operator==(const AddressAttribute& rhs) const;

private:
    const int m_type;
    SocketAddress m_endpoint;
};

class NX_NETWORK_API MappedAddress:
    public AddressAttribute
{
public:
    static constexpr int TYPE = mappedAddress;

    MappedAddress();
    MappedAddress(SocketAddress endpoint);
};

class NX_NETWORK_API AlternateServer:
    public AddressAttribute
{
public:
    static constexpr int TYPE = alternateServer;

    AlternateServer();
    AlternateServer(SocketAddress endpoint);
};

struct NX_NETWORK_API XorMappedAddress: Attribute
{
    static constexpr int TYPE = xorMappedAddress;

    enum
    {
        IPV4 = 1,
        IPV6
    };

    union Ipv6
    {
        struct
        {
            uint64_t hi;
            uint64_t lo;
        } numeric;
        uint16_t words[8];
    };

    XorMappedAddress() = default;
    XorMappedAddress(int port_, uint32_t ipv4_);
    XorMappedAddress(int port_, Ipv6 ipv6_);
    XorMappedAddress(const SocketAddress& addr);

    virtual int getType() const override { return TYPE; }

    SocketAddress addr() const;

    int family = 0;
    int port = 0;
    union
    {
        uint32_t ipv4;
        Ipv6 ipv6;
    } address;  //!< address in host byte order
};

struct NX_NETWORK_API XorPeerAddress: XorMappedAddress
{
    static constexpr int TYPE = xorPeerAddress;

    using XorMappedAddress::XorMappedAddress;

    virtual int getType() const override { return TYPE; }
};

struct NX_NETWORK_API XorRelayedAddress: XorMappedAddress
{
    static constexpr int TYPE = xorRelayedAddress;

    using XorMappedAddress::XorMappedAddress;

    virtual int getType() const override { return TYPE; }
};

struct NX_NETWORK_API BufferedValue
{
    BufferedValue(nx::Buffer buffer_ = nx::Buffer());
    const Buffer& getBuffer() const; //!< Value to serialize
    void setBuffer(Buffer buf);
    std::string getString() const; //!< Convert to string

private:
    Buffer m_buffer;
};

struct NX_NETWORK_API UserName: Attribute, BufferedValue
{
    static constexpr int TYPE = userName;

    UserName(std::string value = std::string());
    virtual int getType() const override { return TYPE; }
};

struct NX_NETWORK_API ErrorCode: Attribute, BufferedValue
{
    static constexpr int TYPE = errorCode;

    ErrorCode(int code_, std::string phrase = std::string());
    virtual int getType() const override { return TYPE; }

    int getClass() const { return m_code / 100; }
    int getNumber() const { return m_code % 100; }
    int getCode() const { return m_code; }
    const std::string& getReason() const { return m_reason; }

private:
    int m_code;           //!< This value is full error code
    std::string m_reason;  //!< utf8 string, limited to 127 characters
};

struct NX_NETWORK_API FingerPrint: Attribute
{
    static constexpr int TYPE = fingerPrint;

    FingerPrint(uint32_t crc32_ = 0);
    virtual int getType() const override { return TYPE; }
    uint32_t getCrc32() const { return crc32; }

private:
    uint32_t crc32;
};

struct NX_NETWORK_API MessageIntegrity: Attribute, BufferedValue
{
    static constexpr int TYPE = messageIntegrity;
    static constexpr int SIZE = 20;

    MessageIntegrity(Buffer hmac = Buffer(SIZE, 0));
    virtual int getType() const override { return TYPE; }
};

struct NX_NETWORK_API Nonce: Attribute, BufferedValue
{
    static constexpr int TYPE = nonce;

    Nonce(Buffer nonce = Buffer());
    virtual int getType() const override { return TYPE; }
};

struct NX_NETWORK_API Unknown: Attribute, BufferedValue
{
    static constexpr int TYPE = unknown;

    Unknown(int userType_, nx::Buffer value_ = nx::Buffer());
    virtual int getType() const override { return userType; }

private:
    int userType;
};

/** Base class for attributes with integer value */
struct NX_NETWORK_API IntAttribute: Unknown
{
    IntAttribute(int userType, int value = 0);

    int value() const;
};

struct NX_NETWORK_API Int64Attribute: Unknown
{
    Int64Attribute(int userType, std::int64_t value = 0);

    std::int64_t value() const;
};

struct NX_NETWORK_API Priority: IntAttribute
{
    static const int TYPE = priority;

    Priority(int value = 0);
    virtual int getType() const override { return TYPE; }
};

struct NX_NETWORK_API Lifetime: IntAttribute
{
    static const int TYPE = lifetime;

    Lifetime(int value = 0);
    virtual int getType() const override { return TYPE; }
};

struct NX_NETWORK_API UseCandidate: Unknown
{
    static const int TYPE = useCandidate;

    UseCandidate(nx::Buffer value_ = nx::Buffer()): Unknown(TYPE, value_) {}
    virtual int getType() const override { return TYPE; }
};

struct NX_NETWORK_API IceControlled: Unknown
{
    static const int TYPE = iceControlled;

    IceControlled(nx::Buffer value_ = nx::Buffer()): Unknown(TYPE, value_) {}
    virtual int getType() const override { return TYPE; }
};

struct NX_NETWORK_API IceControlling: Unknown
{
    static const int TYPE = iceControlling;

    IceControlling(nx::Buffer value_ = nx::Buffer()): Unknown(TYPE, value_) {}
    virtual int getType() const override { return TYPE; }
};

struct NX_NETWORK_API Realm: Unknown
{
    static const int TYPE = realm;

    Realm(nx::Buffer value_ = nx::Buffer()): Unknown(TYPE, value_) {}
    virtual int getType() const override { return TYPE; }
};

struct NX_NETWORK_API ThirdPartyAuthorization: Unknown
{
    static const int TYPE = thirdPartyAuthorization;

    ThirdPartyAuthorization(nx::Buffer value_ = nx::Buffer()): Unknown(TYPE, value_) {}
    virtual int getType() const override { return TYPE; }
};

struct NX_NETWORK_API AccessToken: Unknown
{
    static const int TYPE = accessToken;

    AccessToken(nx::Buffer value_ = nx::Buffer()): Unknown(TYPE, value_) {}
    virtual int getType() const override { return TYPE; }
};

struct NX_NETWORK_API Data: Unknown
{
    static const int TYPE = data;

    Data(nx::Buffer value_ = nx::Buffer()): Unknown(TYPE, value_) {}
    virtual int getType() const override { return TYPE; }
};

struct NX_NETWORK_API RequestedTransport: Unknown
{
    static const int TYPE = requestedTransport;
    /* UDP (0x11) is the only possible value for this attribute. */
    enum class Transport: char
    {
        udp = 0x11,
        tcp = 0x6
    };

    RequestedTransport(Transport transport = Transport::udp);
    virtual int getType() const override { return TYPE; }
};

} // namespace attrs

} // namespace stun
} // namespace network
} // namespace nx
