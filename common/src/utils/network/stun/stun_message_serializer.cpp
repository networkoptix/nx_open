/**********************************************************
* 19 dec 2013
* a.kolesnikov
***********************************************************/

#include "stun_message_serializer.h"
#include <QtEndian>
#include <bitset>

#include <boost/crc.hpp>

namespace nx_stun {
using namespace attr;

class MessageSerializer::MessageSerializerBuffer {
public:
    MessageSerializerBuffer( nx::Buffer* buffer ) :
        buffer_(buffer),
        header_length_(NULL){}

    void* WriteUint16( std::uint16_t value );
    void* WriteUint32( std::uint32_t value );
    void* WriteIPV6Address( const std::uint16_t* value );
    void* WriteByte( char byte );
    void* WriteBytes( const char* bytes , std::size_t length );
    void* Poke( std::size_t size );
    std::size_t position() const {
        return static_cast<std::size_t>(buffer_->size());
    }
    std::size_t size() const {
        return buffer_->size();
    }
    // Use this function to set up the message length position 
    std::uint16_t* WriteMessageLength() {
        Q_ASSERT(header_length_ == NULL);
        void* ret = Poke(2);
        if( ret == NULL ) return NULL;
        header_length_ = reinterpret_cast<std::uint16_t*>(ret);
        return header_length_;
    }
    std::uint16_t* WriteMessageLength( std::uint16_t length ) {
        Q_ASSERT(header_length_ != NULL);
        qToBigEndian(length,reinterpret_cast<uchar*>(header_length_));
        return header_length_;
    }
    const nx::Buffer* buffer() const {
        return buffer_;
    }
private:
    nx::Buffer* buffer_;
    std::uint16_t* header_length_;
};

void* MessageSerializer::MessageSerializerBuffer::Poke( std::size_t size ) {
    if( size + buffer_->size() > buffer_->capacity() ) 
        return NULL;
    else {
        void* ret = buffer_->data() + buffer_->size();
        buffer_->resize( buffer_->size() + static_cast<int>(size) );
        return ret;
    }
}
// Write the uint16 value into the buffer with bytes order swap 
void* MessageSerializer::MessageSerializerBuffer::WriteUint16( std::uint16_t value ) {
    void* ret = Poke(sizeof(std::uint16_t));
    if( ret == NULL ) {
        return NULL;
    } else {
        qToBigEndian(value,reinterpret_cast<uchar*>(ret));
        return ret;
    }
}

void* MessageSerializer::MessageSerializerBuffer::WriteIPV6Address( const std::uint16_t* value ) {
    void* buffer = Poke( 16 );
    if( buffer == NULL ) return NULL;
    for( std::size_t i = 0 ; i < 8 ; ++i ) {
        qToBigEndian(value[i],reinterpret_cast<uchar*>(reinterpret_cast<std::uint16_t*>(buffer) + i));
    }
    return buffer;
}

void* MessageSerializer::MessageSerializerBuffer::WriteUint32( std::uint32_t value ) {
    void* ret = Poke(sizeof(std::uint32_t));
    if( ret == NULL ) {
        return NULL;
    } else {
        qToBigEndian(value,reinterpret_cast<uchar*>(ret));
        return ret;
    }
}
// Write the byte into the specific position
void* MessageSerializer::MessageSerializerBuffer::WriteByte( char byte ) {
    void* ret = Poke(1);
    if( ret == NULL ) {
        return NULL;
    } else {
        *reinterpret_cast<char*>(ret) = byte;
        return ret;
    }
}

void* MessageSerializer::MessageSerializerBuffer::WriteBytes( const char* bytes , std::size_t length ) {
    void* ret = Poke(length);
    if( ret == NULL ) {
        return NULL;
    } else {
        memcpy(ret,bytes,length);
        return ret;
    }
}

// Serialization class implementation
nx_api::SerializerState::Type MessageSerializer::serializeHeaderInitial( MessageSerializerBuffer* buffer ) {
    std::bitset<16> initial(0);
    // 1. Setting the method bits. 
    initial[0] = message_.header.method & (1<<0); 
    initial[1] = message_.header.method & (1<<1); 
    initial[2] = message_.header.method & (1<<2);
    initial[3] = message_.header.method & (1<<3);
    initial[5] = message_.header.method & (1<<4); // skip the 5th bit
    initial[6] = message_.header.method & (1<<5);
    initial[7] = message_.header.method & (1<<6); 
    initial[9] = message_.header.method & (1<<7); // skip the 9th bit
    initial[10]= message_.header.method & (1<<8);
    initial[11]= message_.header.method & (1<<9);
    initial[12]= message_.header.method & (1<<10);
    initial[13]= message_.header.method & (1<<11);
    // 2. Setting the class bits
    int message_class = static_cast<int>(message_.header.messageClass);
    initial[4] = message_class & (1<<0);
    initial[8] = message_class & (1<<1);
    // 3. write to the buffer
    if( buffer->WriteUint16(static_cast<std::uint16_t>(initial.to_ulong())) == NULL ) {
        return nx_api::SerializerState::needMoreBufferSpace;
    } else {
        return nx_api::SerializerState::done;
    }
}

nx_api::SerializerState::Type MessageSerializer::serializeHeaderLengthStart( MessageSerializerBuffer* buffer ) {
    if( buffer->WriteMessageLength() == NULL ) 
        return nx_api::SerializerState::needMoreBufferSpace;
    else
        return nx_api::SerializerState::done;
}

nx_api::SerializerState::Type MessageSerializer::serializeMagicCookieAndTransactionID( MessageSerializerBuffer* buffer ) {
    // Magic cookie
    if( buffer->WriteUint32( MAGIC_COOKIE ) == NULL ) {
        return nx_api::SerializerState::needMoreBufferSpace;
    }
    // Transaction ID
    if( buffer->WriteBytes( message_.header.transactionID.bytes,TransactionID::TRANSACTION_ID_LENGTH) == NULL ) {
        return nx_api::SerializerState::needMoreBufferSpace;
    }
    return nx_api::SerializerState::done;
}

nx_api::SerializerState::Type MessageSerializer::serializeHeader( MessageSerializerBuffer* buffer ) {
    if( serializeHeaderInitial(buffer) == nx_api::SerializerState::needMoreBufferSpace )
        return nx_api::SerializerState::needMoreBufferSpace;
    if( serializeHeaderLengthStart(buffer) == nx_api::SerializerState::needMoreBufferSpace )
        return nx_api::SerializerState::needMoreBufferSpace;
    if( serializeMagicCookieAndTransactionID(buffer) == nx_api::SerializerState::needMoreBufferSpace )
        return nx_api::SerializerState::needMoreBufferSpace;
    return nx_api::SerializerState::done;
}

nx_api::SerializerState::Type MessageSerializer::serializeAttributeTypeAndLength( MessageSerializerBuffer* buffer , const attr::Attribute* attribute  , std::uint16_t** value_pos ) {
    if( buffer->WriteUint16(static_cast<std::uint16_t>(attribute->type)) == NULL ) {
        return nx_api::SerializerState::needMoreBufferSpace;
    }
    *value_pos = reinterpret_cast<std::uint16_t*>(buffer->WriteUint16(static_cast<std::uint16_t>(attribute->length)));
    if( *value_pos == NULL ) {
        return nx_api::SerializerState::needMoreBufferSpace;
    }
    return nx_api::SerializerState::done;
}

nx_api::SerializerState::Type MessageSerializer::serializeAttributeValue( MessageSerializerBuffer* buffer ,const attr::Attribute* attribute , std::size_t* value ) {
    switch( attribute->type ) {
    case AttributeType::errorCode:
        return serializeAttributeValue_ErrorCode( buffer , static_cast<const ErrorCode*>(attribute) ,value);
    case AttributeType::fingerprint:
        return serializeAttributeValue_Fingerprint( buffer , static_cast<const FingerPrint*>(attribute) ,value);
    case AttributeType::xorMappedAddress:
        return serializeAttributeValue_XORMappedAddress( buffer , static_cast<const XorMappedAddress*>(attribute) ,value);
    case AttributeType::messageIntegrity:
        return serializeAttributeValue_MessageIntegrity( buffer , static_cast<const MessageIntegrity*>(attribute) ,value);
    case AttributeType::unknownAttribute:
        return serializeAttributeValue_UnknownAttribute( buffer , static_cast<const UnknownAttribute*>(attribute) ,value);
    default: Q_ASSERT(0); return nx_api::SerializerState::done;
    }
}

nx_api::SerializerState::Type MessageSerializer::serializeAttributeValue_XORMappedAddress( MessageSerializerBuffer* buffer ,const attr::XorMappedAddress* attribute , std::size_t* value ) {
    Q_ASSERT( attribute->family == XorMappedAddress::IPV4 || attribute->family == XorMappedAddress::IPV6 );
    std::size_t cur_pos = buffer->position();
    if( buffer->WriteUint16(attribute->family) == NULL ) 
        return nx_api::SerializerState::needMoreBufferSpace;
    // Writing the port to the output stream. Based on RFC , the port value needs to be XOR with
    // the high part of the MAGIC COOKIE value and then convert to the network byte order.
    if( buffer->WriteUint16( attribute->port ^ MAGIC_COOKIE_HIGH ) == NULL )
        return nx_api::SerializerState::needMoreBufferSpace;
    if( attribute->family == XorMappedAddress::IPV4 ) {
        if( buffer->WriteUint32(attribute->address.ipv4 ^ MAGIC_COOKIE) == NULL )
            return nx_api::SerializerState::needMoreBufferSpace;
    } else {
        std::uint16_t xor_addr[8];
        xor_addr[0] = attribute->address.ipv6.array[0] ^ MAGIC_COOKIE_LOW;
        xor_addr[1] = attribute->address.ipv6.array[1] ^ MAGIC_COOKIE_HIGH;
        // XOR for the transaction id
        for( std::size_t i = 2 ; i < 8 ; ++i ) {
            xor_addr[i] = *reinterpret_cast<std::uint16_t*>(message_.header.transactionID.bytes+(i-2)*2) ^
                attribute->address.ipv6.array[i];
        }

        if( buffer->WriteIPV6Address(xor_addr) == NULL )
            return nx_api::SerializerState::needMoreBufferSpace;
    }
    *value = buffer->position()-cur_pos;
    return nx_api::SerializerState::done;
}

nx_api::SerializerState::Type MessageSerializer::serializeAttributeValue_Fingerprint( MessageSerializerBuffer* buffer ,const attr::FingerPrint* attribute , std::size_t* value ) {
    Q_ASSERT( buffer->size() >= 24 ); // Header + FingerprintHeader
    // Ignore original FingerPrint message
    Q_UNUSED(attribute);
    boost::crc_32_type crc32;
    // The RFC says that we MUST set the length field in header correctly so it means
    // The length should cover the CRC32 attributes, the length is 4+4 = 8, and the 
    // buffer currently only contains the header for Fingerprint but not the value,so
    // we need to fix the length in the buffer here.
    buffer->WriteMessageLength( static_cast<std::uint16_t>(buffer->size() + 4) );
    // Now we calculate the CRC32 value , since the buffer has 4 bytes which is not needed
    // so we must ignore those bytes in the buffer.
    crc32.process_block( buffer->buffer()->constData() , buffer->buffer()->constData() + buffer->size()-4 );
    // Get CRC result value
    std::uint32_t val = static_cast<std::uint32_t>( crc32.checksum() );
    // Do the XOR operation on it
    std::uint32_t xor_result = 0;
    xor_result |= (val & 0x000000ff) ^ STUN_FINGERPRINT_XORMASK[0];
    xor_result |= (val & 0x0000ff00) ^ STUN_FINGERPRINT_XORMASK[1];
    xor_result |= (val & 0x00ff0000) ^ STUN_FINGERPRINT_XORMASK[2];
    xor_result |= (val & 0xff000000) ^ STUN_FINGERPRINT_XORMASK[3];
    // Serialize this message into the buffer body
    buffer->WriteUint32(xor_result);
    *value = 4;
    return nx_api::SerializerState::done;
}

nx_api::SerializerState::Type MessageSerializer::serializeAttributeValue_MessageIntegrity( MessageSerializerBuffer* ,const attr::MessageIntegrity* , std::size_t* ) {
    Q_ASSERT(0);
    // Needs username/password to implement this , I don't know how to do it now :(
    return nx_api::SerializerState::done;
}

nx_api::SerializerState::Type MessageSerializer::serializeAttributeValue_UnknownAttribute( MessageSerializerBuffer* buffer ,const attr::UnknownAttribute* attribute , std::size_t* value ) {
    std::size_t cur_pos = buffer->position();
    if( buffer->WriteBytes( attribute->value.constData() , attribute->value.size() ) == NULL ) 
        return nx_api::SerializerState::needMoreBufferSpace ;
    // The size of the STUN attributes should be the size before padding bytes
    *value = buffer->position()-cur_pos;
    // Padding the UnknownAttributes to the boundary of 4
    std::size_t padding_size = calculatePaddingSize(attribute->value.size());
    for( std::size_t i = attribute->value.size() ; i < padding_size ; ++i ) {
        if( buffer->WriteByte(0) == NULL )
            return nx_api::SerializerState::needMoreBufferSpace;
    }
    return nx_api::SerializerState::done;
}

nx_api::SerializerState::Type MessageSerializer::serializeAttributeValue_ErrorCode( MessageSerializerBuffer* buffer ,const attr::ErrorCode*  attribute , std::size_t* value ) {
    std::size_t cur_pos = buffer->position();
    std::uint32_t error_header = attribute->code % 100;
    // We don't use attribute->_class value since we can get what we want from code
    // but we check the validation here for the attribute->_class value
    error_header |= (attribute->code / 100)<<8;
    if( buffer->WriteUint32(error_header) == NULL )
        return nx_api::SerializerState::needMoreBufferSpace;
    // UTF8 string 
    QByteArray utf8_bytes = QString::fromStdString(attribute->reasonPhrase).toUtf8();
    if( buffer->WriteBytes( utf8_bytes.constData() , utf8_bytes.size() ) == NULL )
        return nx_api::SerializerState::needMoreBufferSpace;
    *value = buffer->position()-cur_pos;
    // Padding
    std::size_t padding_size = calculatePaddingSize(utf8_bytes.size());
    for( std::size_t i = utf8_bytes.size() ; i < padding_size ; ++i ) {
        if( buffer->WriteByte(0) == NULL )
            return nx_api::SerializerState::needMoreBufferSpace;
    }
    return nx_api::SerializerState::done;
}

bool MessageSerializer::travelAllAttributes( const std::function<bool(const attr::Attribute*)>& callback ) {
    Message::AttributesMap::const_iterator message_integrity =
        message_.attributes.find(AttributeType::messageIntegrity);
    Message::AttributesMap::const_iterator fingerprint =
        message_.attributes.find(AttributeType::fingerprint);
    for( Message::AttributesMap::const_iterator ib = message_.attributes.cbegin() ; ib != message_.attributes.cend() ; ++ib ) {
        if( ib != message_integrity && ib != fingerprint ) {
            if(!callback(ib->second.get())) 
                return false;
        }
    }
    if( message_integrity != message_.attributes.cend() ) {
        if( !callback(message_integrity->second.get()) )
            return false;
    }
    if( fingerprint != message_.attributes.cend() ) {
        if( !callback(fingerprint->second.get()) )
            return false;
    }
    return true;
}

nx_api::SerializerState::Type MessageSerializer::serializeAttributes( MessageSerializerBuffer* buffer , std::uint16_t* length ) {
    bool ret = travelAllAttributes(
        [length,this,buffer]( const Attribute* attribute ) {
            std::uint16_t* attribute_len;
            std::size_t attribute_value_size;
            if( serializeAttributeTypeAndLength(buffer,attribute,&attribute_len) == nx_api::SerializerState::needMoreBufferSpace ||
                serializeAttributeValue(buffer,attribute,&attribute_value_size) == nx_api::SerializerState::needMoreBufferSpace ) {
                    return false;
            }
            // Checking for really large message may round our body size 
            std::size_t padding_attribute_value_size = calculatePaddingSize(attribute_value_size);
            Q_ASSERT( padding_attribute_value_size + 4 + *length <= std::numeric_limits<std::uint16_t>::max() );
            qToBigEndian(static_cast<std::uint16_t>(attribute_value_size),reinterpret_cast<uchar*>(attribute_len));
            *length += static_cast<std::uint16_t>(4 + padding_attribute_value_size);
            return true;
        });
    return ret ? nx_api::SerializerState::done : nx_api::SerializerState::needMoreBufferSpace;
}

bool MessageSerializer::checkMessageIntegratiy() {
    // We just do some manually testing for the message validation and all the rules for
    // testing is based on the RFC. 
    // 1. Header
    // Checking the method and it only has 12 bits in the message
    if( message_.header.method <0 || message_.header.method >= 1<<12 ) {
        return false;
    }
    // 2. Checking the attributes 
    Message::AttributesMap::const_iterator ib = message_.attributes.find(AttributeType::fingerprint);
    if( ib != message_.attributes.end() && ++ib != message_.attributes.end() ) {
        return false;
    }
    ib = message_.attributes.find(AttributeType::messageIntegrity);
    if( ib != message_.attributes.end() && ++ib != message_.attributes.end() ) {
        return false;
    }
    // 3. Checking the validation for specific attributes
    // ErrorCode message
    ib = message_.attributes.find(AttributeType::errorCode);
    if( ib !=  message_.attributes.end() ) {
        // Checking the error code message
        ErrorCode* error_code = static_cast<ErrorCode*>(
            ib->second.get());
        if( error_code->_class <3 || error_code->_class >6 )
            return false;
        if( error_code->code < 300 || error_code->code >= 700 )
            return false;
        // class code should match the hundreds of full error code
        if( error_code->_class != (error_code->code/100) )
            return false;
        // RFC: The reason phrase string will at most be 127 characters
        if( error_code->reasonPhrase.size() > 127 )
            return false;
    }
    return true;
}

nx_api::SerializerState::Type MessageSerializer::serialize( nx::Buffer* const user_buffer, size_t* const bytesWritten ) {
    Q_ASSERT(initialized_ && checkMessageIntegratiy());
    Q_ASSERT(user_buffer->size() == 0 && user_buffer->capacity() != 0);
    MessageSerializerBuffer buffer(user_buffer);
    *bytesWritten = user_buffer->size();
    // header serialization
    if( serializeHeader(&buffer) == nx_api::SerializerState::needMoreBufferSpace )
        return nx_api::SerializerState::needMoreBufferSpace;
    // attributes serialization
    std::uint16_t length = 0;
    if( serializeAttributes(&buffer,&length) == nx_api::SerializerState::needMoreBufferSpace ) {
        return nx_api::SerializerState::needMoreBufferSpace;
    }
    // setting the header value 
    buffer.WriteMessageLength(length);
    initialized_ = false;
    *bytesWritten = user_buffer->size() - *bytesWritten;
    return nx_api::SerializerState::done;
}

}// namespace nx_api
