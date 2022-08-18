// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "message_serializer.h"

#include <bitset>

#include <boost/crc.hpp>

#include <nx/utils/string.h>

#include "parse_utils.h"

namespace nx {
namespace network {
namespace stun {

static constexpr int kHeaderSize = 20;

using namespace attrs;

void MessageSerializer::setMessage(const Message* message)
{
    m_message = message;
    m_initialized = true;
}

// Serialization class implementation
server::SerializerState MessageSerializer::serializeHeaderInitial(
    MessageSerializerBuffer* buffer)
{
    std::bitset<16> initial(0);
    // 1. Setting the method bits.
    initial[0] = m_message->header.method & (1 << 0);
    initial[1] = m_message->header.method & (1 << 1);
    initial[2] = m_message->header.method & (1 << 2);
    initial[3] = m_message->header.method & (1 << 3);
    initial[5] = m_message->header.method & (1 << 4); // skip the 5th bit
    initial[6] = m_message->header.method & (1 << 5);
    initial[7] = m_message->header.method & (1 << 6);
    initial[9] = m_message->header.method & (1 << 7); // skip the 9th bit
    initial[10] = m_message->header.method & (1 << 8);
    initial[11] = m_message->header.method & (1 << 9);
    initial[12] = m_message->header.method & (1 << 10);
    initial[13] = m_message->header.method & (1 << 11);
    // 2. Setting the class bits
    int message_class = static_cast<int>(m_message->header.messageClass);
    initial[4] = message_class & (1 << 0);
    initial[8] = message_class & (1 << 1);
    // 3. write to the buffer
    if (buffer->WriteUint16(static_cast<std::uint16_t>(initial.to_ulong())) == NULL)
    {
        return server::SerializerState::needMoreBufferSpace;
    }
    else
    {
        return server::SerializerState::done;
    }
}

server::SerializerState MessageSerializer::serializeHeaderLengthStart(
    MessageSerializerBuffer* buffer)
{
    if (buffer->WriteMessageLength() == NULL)
        return server::SerializerState::needMoreBufferSpace;
    else
        return server::SerializerState::done;
}

server::SerializerState MessageSerializer::serializeMagicCookieAndTransactionID(
    MessageSerializerBuffer* buffer)
{
    // Magic cookie
    if (buffer->WriteUint32(MAGIC_COOKIE) == NULL)
    {
        return server::SerializerState::needMoreBufferSpace;
    }

    NX_ASSERT(m_message->header.transactionId.size() == Header::TRANSACTION_ID_SIZE);
    // Transaction ID
    if (buffer->WriteBytes(m_message->header.transactionId.data(),
        m_message->header.transactionId.size()) == NULL)
    {
        return server::SerializerState::needMoreBufferSpace;
    }
    return server::SerializerState::done;
}

server::SerializerState MessageSerializer::serializeHeader(
    MessageSerializerBuffer* buffer)
{
    NX_ASSERT(m_message->header.messageClass != MessageClass::unknown);
    NX_ASSERT(m_message->header.method != MethodType::invalid);
    NX_ASSERT(m_message->header.transactionId != Header::nullTransactionId);
    if (serializeHeaderInitial(buffer) == server::SerializerState::needMoreBufferSpace)
        return server::SerializerState::needMoreBufferSpace;
    if (serializeHeaderLengthStart(buffer) == server::SerializerState::needMoreBufferSpace)
        return server::SerializerState::needMoreBufferSpace;
    if (serializeMagicCookieAndTransactionID(buffer) == server::SerializerState::needMoreBufferSpace)
        return server::SerializerState::needMoreBufferSpace;
    return server::SerializerState::done;
}

server::SerializerState MessageSerializer::serializeAttributeTypeAndLength(
    MessageSerializerBuffer* buffer,
    int attrType,
    std::uint16_t** lenPtr)
{
    if (buffer->WriteUint16(static_cast<std::uint16_t>(attrType)) == NULL)
        return server::SerializerState::needMoreBufferSpace;

    // NOTE: actual attribute lenght gets rewrited in /fn serializeAttributes
    // TODO: refactor to get rig of this dummy place holder
    *lenPtr = reinterpret_cast<std::uint16_t*>(buffer->WriteUint16(0));
    if (*lenPtr == NULL)
        return server::SerializerState::needMoreBufferSpace;

    return server::SerializerState::done;
}

server::SerializerState MessageSerializer::serializeAttributeValue(
    MessageSerializerBuffer* buffer,
    const attrs::Attribute* attribute,
    std::size_t* bytesWritten)
{
    switch (attribute->getType())
    {
        case attrs::errorCode:
            return serializeAttributeValue_ErrorCode(buffer, *static_cast<const ErrorCode*>(attribute), bytesWritten);

        case attrs::xorMappedAddress:
        case attrs::xorPeerAddress:
        case attrs::xorRelayedAddress:
            return serializeAttributeValue_XORMappedAddress(buffer, *static_cast<const XorMappedAddress*>(attribute), bytesWritten);

        case attrs::messageIntegrity:
            return serializeAttributeValue_Buffer(buffer, *static_cast<const MessageIntegrity*>(attribute), bytesWritten);

        case attrs::userName:
            return serializeAttributeValue_Buffer(buffer, *static_cast<const UserName*>(attribute), bytesWritten);

        case attrs::nonce:
            return serializeAttributeValue_Buffer(buffer, *static_cast<const Nonce*>(attribute), bytesWritten);

        default:
        {
            const SerializableAttribute* serializableAttribute =
                dynamic_cast<const attrs::SerializableAttribute*>(attribute);
            if (serializableAttribute)
                return serializableAttribute->serialize(buffer, bytesWritten);

            if (attribute->getType() > attrs::unknown)
            {
                return serializeAttributeValue_Buffer(
                    buffer, *static_cast<const Unknown*>(attribute), bytesWritten);
            }

            NX_ASSERT(false);
            return server::SerializerState::done;
        }
    }
}

server::SerializerState MessageSerializer::serializeAttributeValue_XORMappedAddress(
    MessageSerializerBuffer* buffer,
    const attrs::XorMappedAddress& attribute,
    std::size_t* bytesWritten)
{
    NX_ASSERT(attribute.family == XorMappedAddress::IPV4 || attribute.family == XorMappedAddress::IPV6);
    std::size_t cur_pos = buffer->position();
    if (buffer->WriteUint16(attribute.family) == NULL)
        return server::SerializerState::needMoreBufferSpace;
    // Writing the port to the output stream. Based on RFC , the port value needs to be XOR with
    // the high part of the MAGIC COOKIE value and then convert to the network byte order.
    if (buffer->WriteUint16(attribute.port ^ MAGIC_COOKIE_HIGH) == NULL)
        return server::SerializerState::needMoreBufferSpace;
    if (attribute.family == XorMappedAddress::IPV4)
    {
        if (buffer->WriteUint32(attribute.address.ipv4 ^ MAGIC_COOKIE) == NULL)
            return server::SerializerState::needMoreBufferSpace;
    }
    else
    {
        std::uint16_t xor_addr[8];
        xor_addr[0] = attribute.address.ipv6.words[0] ^ MAGIC_COOKIE_LOW;
        xor_addr[1] = attribute.address.ipv6.words[1] ^ MAGIC_COOKIE_HIGH;
        // XOR for the transaction id
        for (std::size_t i = 2; i < 8; ++i)
        {
            const auto tid = m_message->header.transactionId.data() + (i - 2) * 2;
            xor_addr[i] = *reinterpret_cast< const std::uint16_t* >(tid) ^
                attribute.address.ipv6.words[i];
        }

        if (buffer->WriteIPV6Address(xor_addr) == NULL)
            return server::SerializerState::needMoreBufferSpace;
    }
    *bytesWritten = buffer->position() - cur_pos;
    return server::SerializerState::done;
}

server::SerializerState MessageSerializer::serializeAttributeValue_Buffer(
    MessageSerializerBuffer* buffer,
    const attrs::BufferedValue& attribute,
    std::size_t* value)
{
    std::size_t cur_pos = buffer->position();
    if (buffer->WriteBytes(attribute.getBuffer().data(), attribute.getBuffer().size()) == NULL)
        return server::SerializerState::needMoreBufferSpace;
    // The size of the STUN attributes should be the size before padding bytes
    *value = buffer->position() - cur_pos;
    // Padding the UnknownAttributes to the boundary of 4
    std::size_t padding_size = addPadding(attribute.getBuffer().size());
    for (std::size_t i = attribute.getBuffer().size(); i < padding_size; ++i)
    {
        if (buffer->WriteByte(0) == NULL)
            return server::SerializerState::needMoreBufferSpace;
    }
    return server::SerializerState::done;
}

server::SerializerState MessageSerializer::serializeAttributeValue_ErrorCode(
    MessageSerializerBuffer* buffer,
    const attrs::ErrorCode&  attribute,
    std::size_t* value)
{
    std::size_t cur_pos = buffer->position();
    std::uint32_t error_header = (attribute.getClass() << 8) | attribute.getNumber();
    if (buffer->WriteUint32(error_header) == NULL)
        return server::SerializerState::needMoreBufferSpace;
    if (attribute.getBuffer().size() == 0)
    {
        // This is an empty reason phase
        *value = buffer->position() - cur_pos;
        return server::SerializerState::done;
    }
    // UTF8 string
    nx::Buffer utf8_bytes = attribute.getBuffer();
    if (buffer->WriteBytes(utf8_bytes.data(), utf8_bytes.size()) == NULL)
        return server::SerializerState::needMoreBufferSpace;
    *value = buffer->position() - cur_pos;
    // Padding
    std::size_t padding_size = addPadding(utf8_bytes.size());
    for (std::size_t i = utf8_bytes.size(); i < padding_size; ++i)
    {
        if (buffer->WriteByte(0) == NULL)
            return server::SerializerState::needMoreBufferSpace;
    }
    return server::SerializerState::done;
}

bool MessageSerializer::travelAllAttributes(
    const std::function<bool(const attrs::Attribute*)>& callback)
{
    const attrs::Attribute* messageIntegrity = nullptr;
    const attrs::Attribute* fingerprint = nullptr;

    bool failed = false;
    m_message->forEachAttribute(
        [&messageIntegrity, &fingerprint, &callback, &failed](const attrs::Attribute* attr)
        {
            if (attr->getType() == attrs::messageIntegrity)
            {
                messageIntegrity = attr;
            }
            else if (attr->getType() == attrs::fingerPrint)
            {
                fingerprint = attr;
            }
            else
            {
                if (!callback(attr))
                    failed = true;
            }
        });

    if (failed)
        return false;

    if (messageIntegrity != nullptr)
    {
        if (!callback(messageIntegrity))
            return false;
    }

    if (fingerprint != nullptr)
    {
        if (!callback(fingerprint))
            return false;
    }

    return true;
}

server::SerializerState MessageSerializer::serializeAttributes(
    MessageSerializerBuffer* buffer)
{
    int length = 0;
    bool ret = travelAllAttributes(
        [&length, this, buffer](const Attribute* attribute)
        {
            if (attribute->getType() == attrs::fingerPrint)
                return true; // The fingerprint is added separately to the very end.

            std::uint16_t* len = nullptr;
            std::size_t valueSize = 0;
            if (serializeAttributeTypeAndLength(buffer, attribute->getType(), &len) ==
                    server::SerializerState::needMoreBufferSpace ||
                serializeAttributeValue(buffer, attribute, &valueSize) ==
                    server::SerializerState::needMoreBufferSpace)
            {
                return false;
            }

            // Checking for really large message may round our body size
            std::size_t padding_attribute_value_size = addPadding(valueSize);
            NX_ASSERT(padding_attribute_value_size + 4 + length <= std::numeric_limits<std::uint16_t>::max());
            qToBigEndian(static_cast<std::uint16_t>(valueSize), reinterpret_cast<uchar*>(len));
            length += static_cast<std::uint16_t>(4 + padding_attribute_value_size);

            return true;
        });

    return ret ? server::SerializerState::done : server::SerializerState::needMoreBufferSpace;
}

bool MessageSerializer::checkMessageIntegrity()
{
    // We just do some manually testing for the message validation and all the rules for
    // testing is based on the RFC.
    // 1. Header
    // Checking the method and it only has 12 bits in the message
    if (m_message->header.method <0 || m_message->header.method >= 1 << 12)
    {
        return false;
    }
    // 2. Checking the attributes
    // NOTE: the position in the map is not valid for this king of verification
    //auto ib = m_message->attributes.find(attrs::fingerPrint);
    //if( ib != m_message->attributes.end() && ++ib != m_message->attributes.end() ) {
    //    return false;
    //}
    //ib = m_message->attributes.find(attrs::messageIntegrity);
    //if( ib != m_message->attributes.end() && ++ib != m_message->attributes.end() ) {
    //    return false;
    //}
    // 3. Checking the validation for specific attributes
    // ErrorCode message
    const ErrorCode* errorCode = m_message->getAttribute<ErrorCode>(attrs::errorCode);
    if (errorCode != nullptr)
    {
        // Checking the error code message
        if (errorCode->getClass() < 3 || errorCode->getClass() > 6)
            return false;
        if (errorCode->getNumber() < 0 || errorCode->getNumber() >= 99)
            return false;
        // RFC: The reason phrase string will at most be 127 characters.
        if (errorCode->getBuffer().size() > 127)
            return false;
    }

    return true;
}

static const int kDefaultMessageBufferSize = 512;

server::SerializerState MessageSerializer::serialize(
    nx::Buffer* const user_buffer,
    size_t* const bytesWritten)
{
    if (user_buffer->capacity() == 0)
        user_buffer->reserve(kDefaultMessageBufferSize);

    for (int i = 0; ; ++i) //TODO #akolesnikov ensure loop is not inifinite
    {
        if (i > 0)
        {
            user_buffer->resize(0);
            user_buffer->reserve(user_buffer->capacity() * 2);
        }

        NX_ASSERT(m_initialized && checkMessageIntegrity());
        MessageSerializerBuffer buffer(user_buffer);
        *bytesWritten = user_buffer->size();

        if (serializeHeader(&buffer) == server::SerializerState::needMoreBufferSpace)
            continue;

        const auto bufSizeBeforeAttrs = buffer.size();
        if (serializeAttributes(&buffer) == server::SerializerState::needMoreBufferSpace)
            continue;

        if (m_alwaysAddFingerprint ||
            m_message->getAttribute<attrs::Attribute>(attrs::fingerPrint) != nullptr)
        {
            if (!addFingerprint(&buffer))
                continue;
        }

        // setting the header value
        buffer.WriteMessageLength(buffer.size() - bufSizeBeforeAttrs);
        m_initialized = false;
        *bytesWritten = user_buffer->size() - *bytesWritten;

        return server::SerializerState::done;
    }
}

void MessageSerializer::setAlwaysAddFingerprint(bool val)
{
    m_alwaysAddFingerprint = val;
}

nx::Buffer MessageSerializer::serialized(const Message& message)
{
    setMessage(&message);

    nx::Buffer serializedMessage;
    size_t bytesWritten = 0;
    serialize(&serializedMessage, &bytesWritten);

    setMessage(nullptr);

    return serializedMessage;
}

bool MessageSerializer::addFingerprint(MessageSerializerBuffer* buffer)
{
    static constexpr int kSize = 8;

    if (buffer->remainingCapacity() < kSize)
        return false;

    // Assuming that the buffer contains the whole message so far.
    const std::uint32_t fingerprint = calcFingerprint(buffer);

    // Writing the attribute header.
    buffer->WriteUint16(attrs::fingerPrint);
    buffer->WriteUint16(sizeof(fingerprint));

    // Writing the attribute value.
    buffer->WriteUint32(fingerprint);

    return true;
}

std::uint32_t MessageSerializer::calcFingerprint(MessageSerializerBuffer* buffer)
{
    static constexpr int kFingerprintAttrSize = 8;

    // The RFC says that we MUST set the length field in header correctly so it means
    // The length should cover the CRC32 attributes, the length is 4+4 = 8, and the
    // buffer currently only contains the header for Fingerprint but not the value,so
    // we need to fix the length in the buffer here.
    buffer->WriteMessageLength(static_cast<std::uint16_t>(
        buffer->size() + kFingerprintAttrSize - kHeaderSize));

    // Now we calculate the CRC32 value , since the buffer has 4 bytes which is not needed
    // so we must ignore those bytes in the buffer.
    boost::crc_32_type crc32;
    crc32.process_bytes(buffer->buffer()->data(), buffer->size());
    const std::uint32_t val = static_cast<std::uint32_t>(crc32.checksum());

    // Do the XOR operation on it
    std::uint32_t xor_result = 0;
    xor_result |= (val & 0x000000ff) ^ STUN_FINGERPRINT_XORMASK[0];
    xor_result |= (val & 0x0000ff00) ^ STUN_FINGERPRINT_XORMASK[1];
    xor_result |= (val & 0x00ff0000) ^ STUN_FINGERPRINT_XORMASK[2];
    xor_result |= (val & 0xff000000) ^ STUN_FINGERPRINT_XORMASK[3];

    return xor_result;
}

} // namespace stun
} // namespace network
} // namespace nx
