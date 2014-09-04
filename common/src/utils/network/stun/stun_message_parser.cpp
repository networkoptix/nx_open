#include "stun_message_parser.h"
#include "stun_message.h"
#include <QtEndian>
#include <QString>

#include <bitset>
#include <cstdint>
#include <deque>

namespace nx_stun{
using namespace attr;
// This message parser buffer add a simple workaround with the original partial data buffer.
// It will consume all the data in the user buffer, but provide a more consistent interface
// for user. So if the parser stuck at one byte data, the MessageParserBuffer will temporary
// store that one byte and make user buffer drained. However without losing any single bytes
class MessageParser::MessageParserBuffer {
public:
    MessageParserBuffer( const nx::Buffer& buffer ): 
        buffer_(buffer), 
        position_(0){}
    std::uint16_t NextUint16( bool* ok );
    std::uint32_t NextUint32( bool* ok );
    std::uint8_t NextByte( bool* ok );
    void NextBytes( std::uint8_t* bytes , std::size_t sz , bool* ok );
    // Do not modify the temp_buffer_size_ here
    void Clear() {
        temp_buffer_.clear();
    }
    std::size_t position() const {
        return position_;
    }
private:
    bool Ensure( std::size_t byte_size , void* buffer );
    std::deque<char> temp_buffer_;
    const nx::Buffer& buffer_;
    std::size_t position_;
    Q_DISABLE_COPY(MessageParserBuffer)
};

// Ensure will check the temporary buffer and also the buffer in nx::Buffer.
// Additionally, if ensure cannot ensure the buffer size, it will still drain
// the original buffer , so the user buffer will always be consumed over .
bool MessageParser::MessageParserBuffer::Ensure( std::size_t byte_size , void* buffer ) {
    // Get the available bytes that we can read from the stream
    std::size_t available_bytes = temp_buffer_.size() + buffer_.size() - position_;
    if( available_bytes < byte_size ) {
        // Drain the user buffer here
        for( std::size_t i = position_ ; i < buffer_.size() ; ++i )
            temp_buffer_.push_back(buffer_.at(static_cast<int>(i)));
        // Modify the position pointer here 
        position_ = buffer_.size();
        return false;
    } else {
        std::size_t pos = 0;
        // 1. Drain the buffer from the temporary buffer here
        for( std::size_t i = 0 ; i < temp_buffer_.size() && byte_size != pos; ++i , ++pos) {
            *(reinterpret_cast<char*>(buffer)+pos) = temp_buffer_.front();
            temp_buffer_.pop_front();
        }
        if( byte_size == pos ) {
            return true;
        }
        // 2. Finish the buffer feeding 
        if( pos == 0 ) {
            switch( byte_size ) {
            case 1: 
                *(reinterpret_cast<char*>(buffer)) = buffer_.at(static_cast<int>(position_));
                position_ += 1; 
                return true;
            case 2:
                *(reinterpret_cast<std::uint16_t*>(buffer)) = 
                    *(reinterpret_cast<const std::uint16_t*>(buffer_.constData()+position_));
                position_ += 2;
                return true;
            case 4:
                *(reinterpret_cast<std::uint32_t*>(buffer)) = 
                    *(reinterpret_cast<const std::uint32_t*>(buffer_.constData()+position_));
                position_ += 4;
                return true;
            default: Q_ASSERT(0); return false;
            }
        } else {
            memcpy(reinterpret_cast<char*>(buffer)+pos,buffer_.constData() + position_, byte_size - pos );
            position_ += byte_size - pos;
            return true;
        }
    }
}

std::uint16_t MessageParser::MessageParserBuffer::NextUint16( bool* ok ) {
    std::uint16_t value;
    if( !Ensure(sizeof(quint16),&value) ) {
        *ok = false; return 0;
    } 
    *ok = true;
    return qFromBigEndian(value);
}

std::uint32_t MessageParser::MessageParserBuffer::NextUint32( bool* ok ) {
    std::uint32_t value;
    if( !Ensure(sizeof(quint32),&value) ) {
        *ok = false; return 0;
    }
    *ok = true;
    return qFromBigEndian(value);
}

std::uint8_t MessageParser::MessageParserBuffer::NextByte( bool* ok ) {
    std::uint8_t value;
    if( !Ensure(sizeof(quint8),&value) ) {
        *ok = false; return 0;
    }
    *ok = true;
    return value;
}

void MessageParser::MessageParserBuffer::NextBytes( std::uint8_t* bytes , std::size_t sz , bool* ok ) {
    if(!Ensure(sz,bytes)) {
        *ok = false;
        return;
    }
    *ok = true;
}

// Parsing for each specific type
Attribute* MessageParser::parseXORMappedAddress( const nx::Buffer& raw , int length , const Header& header ) {
    enum {
        IPV4 = 1,
        IPV6 
    };

    if( raw.size() < 8 || raw.at(0) != 0 ) 
        return NULL;

    // Parsing the family , can be 1 or 2
    std::unique_ptr<XorMappedAddress> attribute( new XorMappedAddress() );
    attribute->length = length;
    attribute->type = AttributeType::xorMappedAddress;
    if( raw.at(1) != IPV4 && raw.at(1) != IPV6 ) 
        return NULL;
    else 
        attribute->family = raw.at(1);

    // Parsing the port 
    static const std::uint16_t kMagicCookieHigh = static_cast<std::uint16_t>(MAGIC_COOKIE >> 16);
    static const std::uint16_t kMagicCookieLow = static_cast<std::uint16_t>(MAGIC_COOKIE & 0x0000ffff);
    std::uint16_t xor_port = qFromBigEndian(*reinterpret_cast<const std::uint16_t*>(raw.constData() + 2));
    attribute->port = xor_port ^ kMagicCookieHigh;

    // Parsing the address 
    if( attribute->family == IPV4 ) {
        std::uint32_t xor_addr = qFromBigEndian(*reinterpret_cast<const std::uint32_t*>(raw.constData() +4));
        attribute->address.ipv4 = xor_addr ^ MAGIC_COOKIE;
    } else {
        // The RFC doesn't indicate how to concatenate, I just assume it with the natural byte order
        std::uint16_t xor_comp;
        // XOR with high part of MAGIC_COOKIE
        xor_comp = qFromBigEndian(*reinterpret_cast<const std::uint16_t*>(raw.constData()+4));
        attribute->address.ipv6.array[0] = xor_comp ^ kMagicCookieHigh;
        // XOR with low part of MAGIC_COOKIE
        xor_comp = qFromBigEndian(*reinterpret_cast<const std::uint16_t*>(raw.constData()+6));
        attribute->address.ipv6.array[0] = xor_comp ^ kMagicCookieLow;
        // XOR with rest of the transaction id
        const std::size_t transaction_id_word_size = TransactionID::TRANSACTION_ID_LENGTH/2u;
        for( std::size_t i = 0 ; i < transaction_id_word_size ; ++i ) {
            xor_comp = qFromBigEndian(*reinterpret_cast<const std::uint16_t*>(raw.constData()+6+i*2));
            attribute->address.ipv6.array[i+2] = 
                xor_comp ^ *reinterpret_cast<const std::uint16_t*>(header.transactionID.bytes+i*2);
        }
    }
    return attribute.release();
}

Attribute* MessageParser::parseErrorCode( const nx::Buffer& raw , int length , const Header& header ) {
    Q_UNUSED(header);
    // Checking for the reserved bits
    if( *reinterpret_cast<const std::uint16_t*>(raw.constData()) != 0  || raw.size() < 4 )
        return NULL;

    std::uint32_t val( qFromBigEndian(*reinterpret_cast<const std::uint32_t*>(raw.constData())) );
    // The first 21 bits is for reservation, but the RFC says it SHOULD be zero, so ignore it.
    std::bitset<16> value(val & 0x0000ffff);
    // First comes 3 bits class
    std::unique_ptr<ErrorCode> attribute( new ErrorCode() );
    attribute->length = length;
    attribute->type = AttributeType::errorCode;
    attribute->_class =0;
    attribute->_class |= static_cast<int>( value[8] );
    attribute->_class |= static_cast<int>( value[9] ) << 1;
    attribute->_class |= static_cast<int>( value[10]) << 2;
    // Checking the class value for the attribute _class 
    if( attribute->_class < 3 || attribute->_class > 6 ) 
        return NULL;
    // Get the least significant byte from the 32 bits dword
    // but the code must not be the value that is the modular, so we need
    // to compute the class and add it to the code as well
    attribute->code = (val & 0x000000ff);
    if( attribute->code < 0 && attribute->code >= 100 ) 
        return NULL;
    attribute->code+= attribute->_class*100;
    // Parsing the UTF encoded error string 
    std::size_t phrase_length = raw.size() - 4;
    if( phrase_length > 0 ) {
        QByteArray utf8_byte_array( raw.constData() + 4 , static_cast<int>(phrase_length) );
        attribute->reasonPhrase = QString::fromUtf8(utf8_byte_array).toStdString();
    }
    return attribute.release();
}

Attribute* MessageParser::parseFingerprint( const nx::Buffer& raw , int length , const Header& header ) {
    Q_UNUSED(header);
    if( raw.size() != 4 )
        return NULL;
    // We don't do any validation for the message, we just fetch the CRC 32 bits out of the packet
    std::unique_ptr<FingerPrint> attribute( new FingerPrint() );
    attribute->length = length;
    attribute->type = AttributeType::fingerprint;
    attribute->crc32 = *reinterpret_cast<const std::uint32_t*>(raw.constData());
    return attribute.release();
}

Attribute* MessageParser::parseMessageIntegrity( const nx::Buffer& raw , int length , const Header& header ) {
    Q_UNUSED(header);
    if( raw.size() != MessageIntegrity::SHA1_HASH_SIZE )
        return NULL;
    std::unique_ptr<MessageIntegrity> attribute( new MessageIntegrity() );
    attribute->length = length;
    attribute->type = AttributeType::messageIntegrity;
    qCopy( raw.begin() , raw.end() , attribute->hmac.begin() );
    return attribute.release();
}

Attribute* MessageParser::parseUnknownAttribute( const nx::Buffer& raw , int length , const Header& header ) {
    Q_UNUSED(header);
    std::unique_ptr< UnknownAttribute > attribute( new UnknownAttribute() );
    attribute->length = length;
    attribute->type = AttributeType::unknown;
    attribute->value = raw;
    return attribute.release();
}

Attribute* MessageParser::parseValue( const nx::Buffer& raw , int type , int length , const Header& header ) {
    switch( type ) {
    case attr::AttributeType::xorMappedAddress: return parseXORMappedAddress(raw,length,header);
    case attr::AttributeType::errorCode: return parseErrorCode(raw,length,header);
    case attr::AttributeType::messageIntegrity: return parseMessageIntegrity(raw,length,header);
    case attr::AttributeType::fingerprint: return parseFingerprint(raw,length,header);
    default: return parseUnknownAttribute(raw,length,header);
    }
}

int MessageParser::parseHeaderInitialAndType( MessageParserBuffer& buffer ) {
    Q_ASSERT(state_ == HEADER_INITIAL_AND_TYPE);
    bool ok;
    std::bitset<16> value = buffer.NextUint16(&ok);
    if(!ok) {
      return IN_PROGRESS;
    }
    // Checking the initial 2 bits, this 2 bits are the MSB and its follower
    if( value[14] || value[15] ) {
        return FAILED;
    } else {
        // The later 14 bits formed the MessageClass and MessageMethod
        // based on the RFC5389, it looks like this:
        // M M M M M C M M M C M M M M
        // The M represents the Method bits and the C represents the type
        // information bits. Also, we need to take into consideration of the 
        // bit order here.

        // Method 
        header_.message_method = 0;
        header_.message_method |= static_cast<int>(value[0]);
        header_.message_method |= static_cast<int>(value[1])<<1;
        header_.message_method |= static_cast<int>(value[2])<<2;
        header_.message_method |= static_cast<int>(value[3])<<3;
        header_.message_method |= static_cast<int>(value[5])<<4; // skip the 5th C bit
        header_.message_method |= static_cast<int>(value[6])<<5; 
        header_.message_method |= static_cast<int>(value[7])<<6; 
        header_.message_method |= static_cast<int>(value[9])<<7; // skip the 9th C bit
        header_.message_method |= static_cast<int>(value[10])<<8; 
        header_.message_method |= static_cast<int>(value[11])<<9;
        header_.message_method |= static_cast<int>(value[12])<<10;
        header_.message_method |= static_cast<int>(value[13])<<11;

        // Type
        header_.message_class = 0;
        header_.message_class |= static_cast<int>(value[4]);
        header_.message_class |= static_cast<int>(value[8]) <<1;
        state_ = HEADER_LENGTH;
        return SECTION_FINISH;
    }
}

int MessageParser::parseHeaderLength( MessageParserBuffer& buffer ) {
    Q_ASSERT(state_ == HEADER_LENGTH);
    bool ok;
    std::uint16_t val = buffer.NextUint16(&ok);
    if(!ok) {
      return IN_PROGRESS;
    }
    header_.message_length = static_cast<std::size_t>(val);
    // Here we do a simple testing for the message length since based on the RCF
    // the message length must contains the bytes with padding which results in
    // the last 2 bits of the message length should always be zero .
    static const std::size_t kLengthMask = 3;
    if( (header_.message_length & kLengthMask)  != 0 ) {
        // We don't understand such header since the last 2 bits of the length
        // is not zero zero. This is another way to tell if a packet is STUN or not
        return FAILED;
    }
    state_ = HEADER_MAGIC_ID;
    return SECTION_FINISH;
}

int MessageParser::parseHeaderMagicCookie( MessageParserBuffer& buffer ) {
    Q_ASSERT(state_ == HEADER_MAGIC_ID);
    bool ok;
    std::uint32_t magic_id;
    buffer.NextBytes(reinterpret_cast<std::uint8_t*>(&magic_id),
        sizeof(std::uint32_t),&ok);
    if(!ok) {
      return IN_PROGRESS;
    }
    if( MAGIC_COOKIE != magic_id ) 
      return FAILED;
    else {
        state_ = HEADER_TRANSACTION_ID;
        return SECTION_FINISH;
    }
}

int MessageParser::parseHeaderTransactionID( MessageParserBuffer& buffer ) {
    Q_ASSERT(state_ == HEADER_TRANSACTION_ID );
    bool ok;
    buffer.NextBytes( static_cast<std::uint8_t*>(header_.transaction_id.bytes) , TransactionID::TRANSACTION_ID_LENGTH , &ok);
    if(!ok) {
      return IN_PROGRESS;
    }
    // We are finished here so we should populate the Message object right now
    // Set left message length for parsing attributes 
    left_message_length_ = header_.message_length;
    // Populate the header
    output_message_->header.messageClass = static_cast<MessageClass>(header_.message_class);
    output_message_->header.messageLength= static_cast<int>(header_.message_length);
    output_message_->header.method = static_cast<int>(header_.message_method);
    output_message_->header.transactionID = header_.transaction_id;

    state_ = MORE_VALUE;
    return SECTION_FINISH;
}

int MessageParser::parseMoreValue( MessageParserBuffer& buffer ) {
    // We need this function to decide where our state machine gonna go.
    // If after we finished transaction id, we find out that the length
    // of our body is zero, simply means we are done here since no attributes
    // are associated with our body. 
    Q_ASSERT(state_ == MORE_VALUE);
    if( left_message_length_ == 0 ) {
        buffer.Clear();
        attribute_.Clear();
        return FINISH;
    } else {
        attribute_.Clear();
        state_ = ATTRIBUTE_TYPE;
        return SECTION_FINISH;
    }
}

int MessageParser::parseAttributeType( MessageParserBuffer& buffer ) {
    Q_ASSERT(state_ == ATTRIBUTE_TYPE);
    bool ok;
    attribute_.type = buffer.NextUint16(&ok);
    if(!ok) {
      return IN_PROGRESS;
    }
    state_ = ATTRIBUTE_LENGTH;
    return SECTION_FINISH;
}

int MessageParser::parseAttributeLength( MessageParserBuffer& buffer ) {
    Q_ASSERT(state_ == ATTRIBUTE_LENGTH);
    bool ok;
    attribute_.length = buffer.NextUint16(&ok);
    if(!ok) {
      return IN_PROGRESS;
    }
    state_ = ATTRIBUTE_VALUE;
    return SECTION_FINISH;
}

int MessageParser::parseAttributeValueNotAdd( MessageParserBuffer& buffer ) {
    bool ok;
    std::size_t padding_length = calculatePaddingSize(attribute_.length);
    attribute_.value.resize( static_cast<int>(padding_length) );
    buffer.NextBytes(reinterpret_cast<std::uint8_t*>(attribute_.value.data()),padding_length,&ok);
    if( !ok ) 
      return IN_PROGRESS;
    // Modify the left message length field : total size 
    // The value length + 2 bytes type + 2 bytes length 
    left_message_length_ -= 4 + padding_length;
    return SECTION_FINISH;
}

int MessageParser::parseAttributeValue( MessageParserBuffer& buffer ) {
    Q_ASSERT(state_ == ATTRIBUTE_VALUE);
    int ret = parseAttributeValueNotAdd(buffer);
    if( ret != SECTION_FINISH ) return ret;
    Attribute* attr = parseValue( attribute_.value , attribute_.type , attribute_.length , output_message_->header );
    if( attr == NULL ) return FAILED;
    output_message_->attributes.emplace( attr->type , std::unique_ptr<Attribute>(attr) );
    switch( attr->type ) {
    case AttributeType::fingerprint:
        state_ = END_FINGERPRINT;
        break;
    case AttributeType::messageIntegrity:
        state_ = END_MESSAGE_INTEGRITY;
        break;
    default:
        state_ = MORE_VALUE;
    }
    return SECTION_FINISH;
}

int MessageParser::parseEndWithFingerprint( MessageParserBuffer& buffer ) {
    // Finger print MUST BE the last message, if we meet more message than we
    // expected, we just return error that we cannot handle this message now
    Q_UNUSED(buffer);
    if( left_message_length_ != 0 )
        return FAILED;
    return FINISH;
}

int MessageParser::parseEndMessageIntegrity( MessageParserBuffer& buffer ) {
    Q_UNUSED(buffer);
    if( left_message_length_ != 0 ) {
        state_ = ATTRIBUTE_ONLY_ALLOW_FINGERPRINT_TYPE;
        return IN_PROGRESS;
    } else {
        return FINISH;
    }
}

nx_api::ParserState::Type MessageParser::parse( const nx::Buffer& user_buffer , std::size_t* bytes_transferred ) {
    Q_ASSERT( !user_buffer.isEmpty() );
    // Setting up the buffer environment variables
    MessageParserBuffer buffer(user_buffer);
    // Tick the parsing state machine
    do {
        int ret = 0;                                    
        switch(state_)
        {
            case HEADER_INITIAL_AND_TYPE:
                ret = parseHeaderInitialAndType(buffer);
                break;
            case HEADER_LENGTH:
                ret = parseHeaderLength(buffer);
                break;
            case HEADER_MAGIC_ID:
                ret = parseHeaderMagicCookie(buffer);
                break;
            case HEADER_TRANSACTION_ID:
                ret = parseHeaderTransactionID(buffer);
                break;
            case ATTRIBUTE_TYPE:
                ret = parseAttributeType(buffer);
                break;
            case ATTRIBUTE_LENGTH:
                ret = parseAttributeLength(buffer);
                break;
            case ATTRIBUTE_VALUE:
                ret = parseAttributeValue(buffer);
                break;
            case ATTRIBUTE_ONLY_ALLOW_FINGERPRINT_TYPE:
                ret = parseAttributeFingerprintType(buffer);
                break;
            case ATTRIBUTE_ONLY_ALLOW_FINGERPRINT_LENGTH:
                ret = parseAttributeFingerprintLength(buffer);
                break;
            case ATTRIBUTE_ONLY_ALLOW_FINGERPRINT_VALUE:
                ret = parseAttributeFingerprintValue(buffer);
                break;
            case MORE_VALUE:
                ret = parseMoreValue(buffer);
                break;
            case END_FINGERPRINT:
                ret = parseEndWithFingerprint(buffer);
                break;
            case END_MESSAGE_INTEGRITY:
                ret = parseEndMessageIntegrity(buffer);
                break;
            default:
                Q_ASSERT(0);
                return 0;
        }

        switch(ret) {                                       
            case SECTION_FINISH:                                      
                break;  //continuing parsing
            case IN_PROGRESS:   
                *bytes_transferred = buffer.position();
                return nx_api::ParserState::inProgress;     
            case FAILED:
                *bytes_transferred = buffer.position();
                return nx_api::ParserState::failed;         
            case FINISH:
                *bytes_transferred = buffer.position();
                return nx_api::ParserState::done;           
            default :                                       
                Q_ASSERT(0);                                
                return nx_api::ParserState::failed;                                      
        }                                                       
    } while( true );
}

}// namespace nx_stun
