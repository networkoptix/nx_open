/**********************************************************
* 19 dec 2013
* a.kolesnikov
***********************************************************/

#include "stun_message_serializer.h"
#include <QtEndian>
#include <bitset>

namespace nx_stun {
using namespace attr;

class MessageSerializer::MessageSerializerBuffer {
public:
    MessageSerializerBuffer( nx::Buffer* buffer ) :buffer_(buffer){}
    void* WriteUint16( std::uint16_t value );
    void* WriteUint32( std::uint32_t value );
    void* WriteIPV6Address( const std::uint16_t* value );
    void* WriteByte( char byte );
    void* WriteBytes( const char* bytes , std::size_t length );
    void* Poke( std::size_t size );
    std::size_t position() const {
        return static_cast<std::size_t>(buffer_->size());
    }
private:
    nx::Buffer* buffer_;
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

nx_api::SerializerState::Type MessageSerializer::serializeHeaderLengthStart( MessageSerializerBuffer* buffer , std::uint16_t** header_position ) {
    *header_position = reinterpret_cast<std::uint16_t*>(buffer->Poke(sizeof(std::uint16_t)));
    if( *header_position == NULL ) 
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

nx_api::SerializerState::Type MessageSerializer::serializeHeader( MessageSerializerBuffer* buffer , std::uint16_t** header_position ) {
    if( serializeHeaderInitial(buffer) == nx_api::SerializerState::needMoreBufferSpace )
        return nx_api::SerializerState::needMoreBufferSpace;
    if( serializeHeaderLengthStart(buffer,header_position) == nx_api::SerializerState::needMoreBufferSpace )
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

nx_api::SerializerState::Type MessageSerializer::serializeAttributeValue_Fingerprint( MessageSerializerBuffer* ,const attr::FingerPrint* , std::size_t* ) {
    // TODO
    Q_ASSERT(0);
    return nx_api::SerializerState::done;
}

nx_api::SerializerState::Type MessageSerializer::serializeAttributeValue_MessageIntegrity( MessageSerializerBuffer* ,const attr::MessageIntegrity* , std::size_t* ) {
    // TODO
    Q_ASSERT(0);
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
    error_header |= (attribute->code / 100)<<8;
    if( buffer->WriteUint32(error_header) != NULL )
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
    return true;
}

nx_api::SerializerState::Type MessageSerializer::serialize( nx::Buffer* const user_buffer, size_t* const bytesWritten ) {
    Q_ASSERT(initialized_);
    MessageSerializerBuffer buffer(user_buffer);
    *bytesWritten = user_buffer->size();
    // header serialization
    std::uint16_t* header_position;
    if( serializeHeader(&buffer,&header_position) == nx_api::SerializerState::needMoreBufferSpace )
        return nx_api::SerializerState::needMoreBufferSpace;
    // attributes serialization
    std::uint16_t length = 0;
    if( serializeAttributes(&buffer,&length) == nx_api::SerializerState::needMoreBufferSpace ) {
        return nx_api::SerializerState::needMoreBufferSpace;
    }
    // setting the header value 
    qToBigEndian(length,reinterpret_cast<uchar*>(header_position));
    initialized_ = false;
    *bytesWritten = user_buffer->size() - *bytesWritten;
    return nx_api::SerializerState::done;
}

}// namespace nx_api
