#include "message_serializer.h"

#include <QtEndian>
#include <bitset>

#include <boost/crc.hpp>

namespace nx {
namespace stun {

using namespace attrs;

MessageSerializer::MessageSerializer():
    m_message(nullptr),
    m_initialized(false)
{
}

void MessageSerializer::setMessage(const Message* message)
{
    m_message = message;
    m_initialized = true;
}

// Serialization class implementation
nx::network::server::SerializerState MessageSerializer::serializeHeaderInitial(
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
        return nx::network::server::SerializerState::needMoreBufferSpace;
    }
    else
    {
        return nx::network::server::SerializerState::done;
    }
}

nx::network::server::SerializerState MessageSerializer::serializeHeaderLengthStart(
    MessageSerializerBuffer* buffer)
{
    if (buffer->WriteMessageLength() == NULL)
        return nx::network::server::SerializerState::needMoreBufferSpace;
    else
        return nx::network::server::SerializerState::done;
}

nx::network::server::SerializerState MessageSerializer::serializeMagicCookieAndTransactionID(
    MessageSerializerBuffer* buffer)
{
    // Magic cookie
    if (buffer->WriteUint32(MAGIC_COOKIE) == NULL)
    {
        return nx::network::server::SerializerState::needMoreBufferSpace;
    }

    NX_ASSERT(m_message->header.transactionId.size() == Header::TRANSACTION_ID_SIZE);
    // Transaction ID
    if (buffer->WriteBytes(m_message->header.transactionId.data(),
        m_message->header.transactionId.size()) == NULL)
    {
        return nx::network::server::SerializerState::needMoreBufferSpace;
    }
    return nx::network::server::SerializerState::done;
}

nx::network::server::SerializerState MessageSerializer::serializeHeader(
    MessageSerializerBuffer* buffer)
{
    NX_ASSERT(m_message->header.messageClass != MessageClass::unknown);
    NX_ASSERT(m_message->header.method != MethodType::invalid);
    NX_ASSERT(m_message->header.transactionId != Header::nullTransactionId);
    if (serializeHeaderInitial(buffer) == nx::network::server::SerializerState::needMoreBufferSpace)
        return nx::network::server::SerializerState::needMoreBufferSpace;
    if (serializeHeaderLengthStart(buffer) == nx::network::server::SerializerState::needMoreBufferSpace)
        return nx::network::server::SerializerState::needMoreBufferSpace;
    if (serializeMagicCookieAndTransactionID(buffer) == nx::network::server::SerializerState::needMoreBufferSpace)
        return nx::network::server::SerializerState::needMoreBufferSpace;
    return nx::network::server::SerializerState::done;
}

nx::network::server::SerializerState MessageSerializer::serializeAttributeTypeAndLength(
    MessageSerializerBuffer* buffer,
    const attrs::Attribute* attribute,
    std::uint16_t** value_pos)
{
    if (buffer->WriteUint16(static_cast<std::uint16_t>(attribute->getType())) == NULL)
    {
        return nx::network::server::SerializerState::needMoreBufferSpace;
    }

    // NOTE: actual attribute lenght gets rewrited in /fn serializeAttributes
    // TODO: refactor to get rig of this dummy place holder
    *value_pos = reinterpret_cast<std::uint16_t*>(buffer->WriteUint16(0));
    if (*value_pos == NULL)
    {
        return nx::network::server::SerializerState::needMoreBufferSpace;
    }

    return nx::network::server::SerializerState::done;
}

nx::network::server::SerializerState MessageSerializer::serializeAttributeValue(
    MessageSerializerBuffer* buffer,
    const attrs::Attribute* attribute,
    std::size_t* bytesWritten)
{
    switch (attribute->getType())
    {
        case attrs::errorCode:
            return serializeAttributeValue_ErrorCode(buffer, *static_cast<const ErrorCode*>(attribute), bytesWritten);
        case attrs::fingerPrint:
            return serializeAttributeValue_Fingerprint(buffer, *static_cast<const FingerPrint*>(attribute), bytesWritten);
        case attrs::xorMappedAddress:
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
                return serializeAttributeValue_Buffer(buffer, *static_cast<const Unknown*>(attribute), bytesWritten);
            NX_ASSERT(0);
            return nx::network::server::SerializerState::done;
        }
    }
}

nx::network::server::SerializerState MessageSerializer::serializeAttributeValue_XORMappedAddress(
    MessageSerializerBuffer* buffer,
    const attrs::XorMappedAddress& attribute,
    std::size_t* bytesWritten)
{
    NX_ASSERT(attribute.family == XorMappedAddress::IPV4 || attribute.family == XorMappedAddress::IPV6);
    std::size_t cur_pos = buffer->position();
    if (buffer->WriteUint16(attribute.family) == NULL)
        return nx::network::server::SerializerState::needMoreBufferSpace;
    // Writing the port to the output stream. Based on RFC , the port value needs to be XOR with
    // the high part of the MAGIC COOKIE value and then convert to the network byte order.
    if (buffer->WriteUint16(attribute.port ^ MAGIC_COOKIE_HIGH) == NULL)
        return nx::network::server::SerializerState::needMoreBufferSpace;
    if (attribute.family == XorMappedAddress::IPV4)
    {
        if (buffer->WriteUint32(attribute.address.ipv4 ^ MAGIC_COOKIE) == NULL)
            return nx::network::server::SerializerState::needMoreBufferSpace;
    }
    else
    {
        std::uint16_t xor_addr[8];
        xor_addr[0] = attribute.address.ipv6.array[0] ^ MAGIC_COOKIE_LOW;
        xor_addr[1] = attribute.address.ipv6.array[1] ^ MAGIC_COOKIE_HIGH;
        // XOR for the transaction id
        for (std::size_t i = 2; i < 8; ++i)
        {
            const auto tid = m_message->header.transactionId.data() + (i - 2) * 2;
            xor_addr[i] = *reinterpret_cast< const std::uint16_t* >(tid) ^
                attribute.address.ipv6.array[i];
        }

        if (buffer->WriteIPV6Address(xor_addr) == NULL)
            return nx::network::server::SerializerState::needMoreBufferSpace;
    }
    *bytesWritten = buffer->position() - cur_pos;
    return nx::network::server::SerializerState::done;
}

nx::network::server::SerializerState MessageSerializer::serializeAttributeValue_Fingerprint(
    MessageSerializerBuffer* buffer,
    const attrs::FingerPrint& attribute,
    std::size_t* value)
{
    NX_ASSERT(buffer->size() >= 24); // Header + FingerprintHeader
                                     // Ignore original FingerPrint message
    Q_UNUSED(attribute);
    boost::crc_32_type crc32;
    // The RFC says that we MUST set the length field in header correctly so it means
    // The length should cover the CRC32 attributes, the length is 4+4 = 8, and the 
    // buffer currently only contains the header for Fingerprint but not the value,so
    // we need to fix the length in the buffer here.
    buffer->WriteMessageLength(static_cast<std::uint16_t>(buffer->size() + 4));
    // Now we calculate the CRC32 value , since the buffer has 4 bytes which is not needed
    // so we must ignore those bytes in the buffer.
    crc32.process_block(buffer->buffer()->constData(), buffer->buffer()->constData() + buffer->size() - 4);
    // Get CRC result value
    std::uint32_t val = static_cast<std::uint32_t>(crc32.checksum());
    // Do the XOR operation on it
    std::uint32_t xor_result = 0;
    xor_result |= (val & 0x000000ff) ^ STUN_FINGERPRINT_XORMASK[0];
    xor_result |= (val & 0x0000ff00) ^ STUN_FINGERPRINT_XORMASK[1];
    xor_result |= (val & 0x00ff0000) ^ STUN_FINGERPRINT_XORMASK[2];
    xor_result |= (val & 0xff000000) ^ STUN_FINGERPRINT_XORMASK[3];
    // Serialize this message into the buffer body
    buffer->WriteUint32(xor_result);
    *value = 4;
    return nx::network::server::SerializerState::done;
}

nx::network::server::SerializerState MessageSerializer::serializeAttributeValue_Buffer(
    MessageSerializerBuffer* buffer,
    const attrs::BufferedValue& attribute,
    std::size_t* value)
{
    std::size_t cur_pos = buffer->position();
    if (buffer->WriteBytes(attribute.getBuffer().constData(), attribute.getBuffer().size()) == NULL)
        return nx::network::server::SerializerState::needMoreBufferSpace;
    // The size of the STUN attributes should be the size before padding bytes
    *value = buffer->position() - cur_pos;
    // Padding the UnknownAttributes to the boundary of 4
    std::size_t padding_size = calculatePaddingSize(attribute.getBuffer().size());
    for (std::size_t i = attribute.getBuffer().size(); i < padding_size; ++i)
    {
        if (buffer->WriteByte(0) == NULL)
            return nx::network::server::SerializerState::needMoreBufferSpace;
    }
    return nx::network::server::SerializerState::done;
}

nx::network::server::SerializerState MessageSerializer::serializeAttributeValue_ErrorCode(
    MessageSerializerBuffer* buffer,
    const attrs::ErrorCode&  attribute,
    std::size_t* value)
{
    std::size_t cur_pos = buffer->position();
    std::uint32_t error_header = (attribute.getClass() << 8) | attribute.getNumber();
    if (buffer->WriteUint32(error_header) == NULL)
        return nx::network::server::SerializerState::needMoreBufferSpace;
    if (attribute.getBuffer().size() == 0)
    {
        // This is an empty reason phase 
        *value = buffer->position() - cur_pos;
        return nx::network::server::SerializerState::done;
    }
    // UTF8 string 
    nx::Buffer utf8_bytes = attribute.getBuffer();
    if (buffer->WriteBytes(utf8_bytes.constData(), utf8_bytes.size()) == NULL)
        return nx::network::server::SerializerState::needMoreBufferSpace;
    *value = buffer->position() - cur_pos;
    // Padding
    std::size_t padding_size = calculatePaddingSize(utf8_bytes.size());
    for (std::size_t i = utf8_bytes.size(); i < padding_size; ++i)
    {
        if (buffer->WriteByte(0) == NULL)
            return nx::network::server::SerializerState::needMoreBufferSpace;
    }
    return nx::network::server::SerializerState::done;
}

bool MessageSerializer::travelAllAttributes(const std::function<bool(const attrs::Attribute*)>& callback)
{
    auto message_integrity = m_message->attributes.find(attrs::messageIntegrity);
    auto fingerprint = m_message->attributes.find(attrs::fingerPrint);

    for (auto ib = m_message->attributes.cbegin(); ib != m_message->attributes.cend(); ++ib)
    {
        if (ib != message_integrity && ib != fingerprint)
        {
            if (!callback(ib->second.get()))
                return false;
        }
    }
    if (message_integrity != m_message->attributes.cend())
    {
        if (!callback(message_integrity->second.get()))
            return false;
    }
    if (fingerprint != m_message->attributes.cend())
    {
        if (!callback(fingerprint->second.get()))
            return false;
    }
    return true;
}

nx::network::server::SerializerState MessageSerializer::serializeAttributes(
    MessageSerializerBuffer* buffer,
    std::uint16_t* length)
{
    bool ret = travelAllAttributes(
        [length, this, buffer](const Attribute* attribute) {
        std::uint16_t* attribute_len;
        std::size_t attribute_value_size;
        if (serializeAttributeTypeAndLength(buffer, attribute, &attribute_len) == nx::network::server::SerializerState::needMoreBufferSpace ||
            serializeAttributeValue(buffer, attribute, &attribute_value_size) == nx::network::server::SerializerState::needMoreBufferSpace)
        {
            return false;
        }
        // Checking for really large message may round our body size 
        std::size_t padding_attribute_value_size = calculatePaddingSize(attribute_value_size);
        NX_ASSERT(padding_attribute_value_size + 4 + *length <= std::numeric_limits<std::uint16_t>::max());
        qToBigEndian(static_cast<std::uint16_t>(attribute_value_size), reinterpret_cast<uchar*>(attribute_len));
        *length += static_cast<std::uint16_t>(4 + padding_attribute_value_size);
        return true;
    });
    return ret ? nx::network::server::SerializerState::done : nx::network::server::SerializerState::needMoreBufferSpace;
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
    const auto ib = m_message->attributes.find(attrs::errorCode);
    if (ib != m_message->attributes.end())
    {
        // Checking the error code message
        ErrorCode* error_code = static_cast<ErrorCode*>(
            ib->second.get());
        if (error_code->getClass() < 3 || error_code->getClass() > 6)
            return false;
        if (error_code->getNumber() < 0 || error_code->getNumber() >= 99)
            return false;
        // RFC: The reason phrase string will at most be 127 characters
        if (error_code->getBuffer().size() > 127)
            return false;
    }
    return true;
}

static const int kDefaultMessageBufferSize = 128;

nx::network::server::SerializerState MessageSerializer::serialize(
    nx::Buffer* const user_buffer,
    size_t* const bytesWritten)
{
    if (user_buffer->capacity() == 0)
        user_buffer->reserve(kDefaultMessageBufferSize);

    for (int i = 0; ; ++i) //TODO #ak ensure loop is not inifinite
    {
        if (i > 0)
        {
            user_buffer->resize(0);
            user_buffer->reserve(user_buffer->capacity() * 2);
        }

        NX_ASSERT(m_initialized && checkMessageIntegrity());
        MessageSerializerBuffer buffer(user_buffer);
        *bytesWritten = user_buffer->size();
        // header serialization
        if (serializeHeader(&buffer) == nx::network::server::SerializerState::needMoreBufferSpace)
            continue;
        // attributes serialization
        std::uint16_t length = 0;
        if (serializeAttributes(&buffer, &length) == nx::network::server::SerializerState::needMoreBufferSpace)
            continue;
        // setting the header value 
        buffer.WriteMessageLength(length);
        m_initialized = false;
        *bytesWritten = user_buffer->size() - *bytesWritten;
        return nx::network::server::SerializerState::done;
    }
}

nx::Buffer MessageSerializer::serialized(const Message& message)
{
    MessageSerializer serializator;
    serializator.setMessage(&message);
    nx::Buffer serializedMessage;
    size_t bytesWritten = 0;
    serializator.serialize(&serializedMessage, &bytesWritten);
    return serializedMessage;
}

} // namespace stun
} // namespace nx
