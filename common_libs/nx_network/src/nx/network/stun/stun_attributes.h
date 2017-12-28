#pragma once

#include <memory>

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
    userName = 0x0006,
    messageIntegrity = 0x0008,
    errorCode = 0x0009,
    unknown = 0x000A,
    realm = 0x0014,
    nonce = 0x0015,
    xorMappedAddress = 0x0020,

    software = 0x8022,
    alternateServer = 0x8023,
    fingerPrint = 0x8028,

    // Starting value for custom NX attributes in the middle of unasigned range 0xC003-0xFFFF:
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

class NX_NETWORK_API MappedAddress:
    public SerializableAttribute
{
public:
    static const int TYPE = mappedAddress;

    MappedAddress();
    MappedAddress(SocketAddress endpoint);

    virtual int getType() const override;
    virtual nx::network::server::SerializerState serialize(
        MessageSerializerBuffer* buffer,
        std::size_t* bytesWritten) const override;
    virtual bool deserialize(MessageParserBuffer* buffer) override;

    const SocketAddress& endpoint() const;

    bool operator==(const MappedAddress& rhs) const;

private:
    static const int kAddressTypeIpV4 = 1;
    static const int kAddressTypeIpV6 = 2;

    SocketAddress m_endpoint;
};

class NX_NETWORK_API AlternateServer:
    public MappedAddress
{
public:
    static const int TYPE = alternateServer;

    AlternateServer();
    AlternateServer(SocketAddress endpoint);

    virtual int getType() const override;
};

struct NX_NETWORK_API XorMappedAddress: Attribute
{
    static const int TYPE = xorMappedAddress;

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
        uint16_t array[8];
    };

    XorMappedAddress();
    XorMappedAddress(int port_, uint32_t ipv4_);
    XorMappedAddress(int port_, Ipv6 ipv6_);

    virtual int getType() const override { return TYPE; }

    int family;
    int port;
    union
    {
        uint32_t ipv4;
        Ipv6 ipv6;
    } address;  //!< address in host byte order
};

struct NX_NETWORK_API BufferedValue
{
    BufferedValue(nx::Buffer buffer_ = nx::Buffer());
    const Buffer& getBuffer() const; //!< Value to serialize
    void setBuffer(Buffer buf);
    String getString() const; //!< Convert to string

private:
    Buffer m_buffer;
};

struct NX_NETWORK_API UserName: Attribute, BufferedValue
{
    static const int TYPE = userName;

    UserName(const String& value = String());
    virtual int getType() const override { return TYPE; }
};

struct NX_NETWORK_API ErrorCode: Attribute, BufferedValue
{
    static const int TYPE = errorCode;

    ErrorCode(int code_, const nx::String& phrase = nx::String());
    virtual int getType() const override { return TYPE; }

    inline int getClass() const { return m_code / 100; }
    inline int getNumber() const { return m_code % 100; }
    inline int getCode() const { return m_code; }
    inline const nx::String& getReason() const { return m_reason; }

private:
    int m_code;           //!< This value is full error code
    nx::String m_reason;  //!< utf8 string, limited to 127 characters
};

struct NX_NETWORK_API FingerPrint: Attribute
{
    static const int TYPE = fingerPrint;

    FingerPrint(uint32_t crc32_ = 0);
    virtual int getType() const override { return TYPE; }
    uint32_t getCrc32() const { return crc32; }

private:
    uint32_t crc32;
};


struct NX_NETWORK_API MessageIntegrity: Attribute, BufferedValue
{
    static const int TYPE = messageIntegrity;
    static const int SIZE = 20;

    MessageIntegrity(Buffer hmac = Buffer(SIZE, 0));
    virtual int getType() const override { return TYPE; }
};

struct NX_NETWORK_API Nonce: Attribute, BufferedValue
{
    static const int TYPE = nonce;

    Nonce(Buffer nonce = Buffer());
    virtual int getType() const override { return TYPE; }
};

struct NX_NETWORK_API Unknown: Attribute, BufferedValue
{
    static const int TYPE = unknown;

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

} // namespace attrs

} // namespace stun
} // namespace network
} // namespace nx
