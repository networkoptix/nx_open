#include "message_parser.h"

#include <bitset>
#include <cstdint>
#include <deque>
#include <QtEndian>
#include <QString>

namespace nx {
namespace stun {

using namespace attrs;

// This message parser buffer add a simple workaround with the original partial data buffer.
// It will consume all the data in the user buffer, but provide a more consistent interface
// for user. So if the parser stuck at one byte data, the MessageParserBuffer will temporary
// store that one byte and make user buffer drained. However without losing any single bytes
class MessageParser::MessageParserBuffer {
public:
    MessageParserBuffer( std::deque<char>* temp_buffer , const nx::Buffer& buffer ): 
        temp_buffer_(temp_buffer),
        buffer_(buffer), 
        m_position(0){}
    MessageParserBuffer( const nx::Buffer& buffer ): 
        temp_buffer_(NULL),
        buffer_(buffer), 
        m_position(0){}
    std::uint16_t NextUint16( bool* ok );
    std::uint32_t NextUint32( bool* ok );
    std::uint8_t NextByte( bool* ok );
    void NextBytes( char* bytes , std::size_t sz , bool* ok );
    // Do not modify the temp_buffer_size_ here
    void Clear() {
        if( temp_buffer_ != NULL )
            temp_buffer_->clear();
    }
    std::size_t position() const {
        return m_position;
    }
private:
    bool Ensure( std::size_t byte_size , void* buffer );
    std::deque<char>* temp_buffer_;
    const nx::Buffer& buffer_;
    std::size_t m_position;
    Q_DISABLE_COPY(MessageParserBuffer)
};

// Ensure will check the temporary buffer and also the buffer in nx::Buffer.
// Additionally, if ensure cannot ensure the buffer size, it will still drain
// the original buffer , so the user buffer will always be consumed over .
bool MessageParser::MessageParserBuffer::Ensure( std::size_t byte_size , void* buffer ) {
    // Get the available bytes that we can read from the stream
    std::size_t available_bytes = (temp_buffer_ == NULL ? 0 : temp_buffer_->size()) + buffer_.size() - m_position;
    if( available_bytes < byte_size ) {
        // Drain the user buffer here
        if( temp_buffer_ != NULL ) {
            for( int i = m_position ; i < buffer_.size() ; ++i )
                temp_buffer_->push_back(buffer_.at(i));
            // Modify the position pointer here 
            m_position = buffer_.size();
        }
        return false;
    } else {
        std::size_t pos = 0;
        // 1. Drain the buffer from the temporary buffer here
        if( temp_buffer_ != NULL ) {
            const std::size_t temp_buffer_size = temp_buffer_->size();
            for( std::size_t i = 0 ; i < temp_buffer_size && byte_size != pos; ++i , ++pos) {
                *(reinterpret_cast<char*>(buffer)+pos) = temp_buffer_->front();
                temp_buffer_->pop_front();
            }
            if( byte_size == pos ) {
                return true;
            }
        }
        // 2. Finish the buffer feeding 
        memcpy(reinterpret_cast<char*>(buffer)+pos,buffer_.constData() + m_position, byte_size - pos );
        m_position += byte_size - pos;
        return true;
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

void MessageParser::MessageParserBuffer::NextBytes( char* bytes , std::size_t sz , bool* ok ) {
    if(!Ensure(sz,bytes)) {
        *ok = false;
        return;
    }
    *ok = true;
}

// Parsing for each specific type
Attribute* MessageParser::parseXORMappedAddress() {
    if( m_attribute.value.size() < 8 || m_attribute.value.at(0) != 0 )
        return NULL;
    MessageParserBuffer buffer(m_attribute.value);
    bool ok;

    // Parsing the family 
    std::unique_ptr<XorMappedAddress> attribute( new XorMappedAddress() );
    std::uint16_t family = buffer.NextUint16(&ok);
    Q_ASSERT(ok);
    // We only need the lower part of the word.
    family = family & 0x000000ff;
    if( family != XorMappedAddress::IPV4 && family != XorMappedAddress::IPV6 ) {
        return NULL;
    }
    attribute->family = family;
    // Parsing the port 
    std::uint16_t xor_port = buffer.NextUint16(&ok);
    Q_ASSERT(ok);
    attribute->port = xor_port ^ MAGIC_COOKIE_HIGH;

    // Parsing the address 
    if( attribute->family == XorMappedAddress::IPV4 ) {
        std::uint32_t xor_addr = buffer.NextUint32(&ok);
        Q_ASSERT(ok);
        attribute->address.ipv4 = xor_addr ^ MAGIC_COOKIE;
    } else {
        // Ensure the buffer
        if( m_attribute.value.size() != 20 )
            return NULL;
        // The RFC doesn't indicate how to concatenate, I just assume it with the natural byte order
        std::uint16_t xor_comp;
        // XOR with high part of MAGIC_COOKIE
        xor_comp = buffer.NextUint16(&ok);
        Q_ASSERT(ok);
        attribute->address.ipv6.array[0] = xor_comp ^ MAGIC_COOKIE_LOW;
        // XOR with low part of MAGIC_COOKIE
        xor_comp = buffer.NextUint16(&ok);
        Q_ASSERT(ok);
        attribute->address.ipv6.array[1] = xor_comp ^ MAGIC_COOKIE_HIGH;
        // XOR with rest of the transaction id
        for( std::size_t i = 0 ; i < 6 ; ++i ) {
            xor_comp = buffer.NextUint16(&ok);
            Q_ASSERT(ok);
            attribute->address.ipv6.array[i+2] = 
                xor_comp ^ *reinterpret_cast< const std::uint16_t* >(
                        m_header.transactionId.data() + i * 2 );
        }
    }
    return attribute.release();
}

Attribute* MessageParser::parseErrorCode() {
    // Checking for the reserved bits
    if( *reinterpret_cast<const std::uint16_t*>(m_attribute.value.constData()) != 0  || m_attribute.value.size() < 4 )
        return NULL;
    MessageParserBuffer buffer(m_attribute.value);
    bool ok;
    std::uint32_t val = buffer.NextUint32(&ok);
    Q_ASSERT(ok);
    // The first 21 bits is for reservation, but the RFC says it SHOULD be zero, so ignore it.
    std::bitset<16> value(val & 0x0000ffff);

    // First comes 3 bits class
    int _class = 0;
    _class |= static_cast<int>( value[8] );
    _class |= static_cast<int>( value[9] ) << 1;
    _class |= static_cast<int>( value[10]) << 2;
    // Checking the class value for the attribute _class 
    if( _class < 3 || _class > 6 )
        return NULL;
    // Get the least significant byte from the 32 bits dword
    // but the code must not be the value that is the modular, so we need
    // to compute the class and add it to the code as well
    int number = (val & 0x000000ff);
    if( number < 0 && number >= 100 )
        return NULL;
    int code = _class * 100 + number;

    // Parsing the UTF encoded error string 
    std::size_t phrase_length = m_attribute.value.size() - 4;
    nx::String phrase;
    if( phrase_length > 0 ) {
        phrase = nx::String( m_attribute.value.constData() + 4, static_cast<int>(phrase_length) );
        // The RFC says that the decoded UTF8 string should only contain less than 127 characters
        if( phrase.size() > 127 ) {
            return NULL;
        }
    }

    return new ErrorDescription( code, phrase );
}

Attribute* MessageParser::parseFingerprint() {
    if( m_attribute.value.size() != 4 )
        return NULL;

    MessageParserBuffer buffer(m_attribute.value);
    bool ok;
    std::uint32_t xor_crc_32 = buffer.NextUint32(&ok);
    Q_ASSERT(ok);

    // XOR back the value that we get from our CRC32 value
    uint32_t crc32 =
      ((xor_crc_32 & 0x000000ff) ^ STUN_FINGERPRINT_XORMASK[0]) |
      ((xor_crc_32 & 0x0000ff00) ^ STUN_FINGERPRINT_XORMASK[1]) |
      ((xor_crc_32 & 0x00ff0000) ^ STUN_FINGERPRINT_XORMASK[2]) |
      ((xor_crc_32 & 0xff000000) ^ STUN_FINGERPRINT_XORMASK[3]);

    // We don't do any validation for the message, we just fetch the CRC 32 bits out of the packet
    return new FingerPrint( crc32 );
}

Attribute* MessageParser::parseMessageIntegrity() {
    if( m_attribute.value.size() != MessageIntegrity::SHA1_HASH_SIZE )
        return NULL;
    std::unique_ptr<MessageIntegrity> attribute( new MessageIntegrity() );
    qCopy( m_attribute.value.begin() , m_attribute.value.end() , attribute->hmac.begin() );
    return attribute.release();
}

Attribute* MessageParser::parseUnknownAttribute() {
    return new Unknown( m_attribute.type, m_attribute.value );
}

Attribute* MessageParser::parseValue() {
    switch( m_attribute.type ) {
        case attrs::XOR_MAPPED_ADDRESS: return parseXORMappedAddress();
        case attrs::ERROR_CODE:         return parseErrorCode();
        case attrs::MESSAGE_INTEGRITY:  return parseMessageIntegrity();
        case attrs::FINGER_PRINT:       return parseFingerprint();
        default:                        return parseUnknownAttribute();
    }
}

int MessageParser::parseHeaderInitialAndType( MessageParserBuffer& buffer ) {
    Q_ASSERT(m_state == HEADER_INITIAL_AND_TYPE);
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
        m_header.method = 0;
        m_header.method |= static_cast<int>(value[0]);
        m_header.method |= static_cast<int>(value[1])<<1;
        m_header.method |= static_cast<int>(value[2])<<2;
        m_header.method |= static_cast<int>(value[3])<<3;
        m_header.method |= static_cast<int>(value[5])<<4; // skip the 5th C bit
        m_header.method |= static_cast<int>(value[6])<<5;
        m_header.method |= static_cast<int>(value[7])<<6;
        m_header.method |= static_cast<int>(value[9])<<7; // skip the 9th C bit
        m_header.method |= static_cast<int>(value[10])<<8;
        m_header.method |= static_cast<int>(value[11])<<9;
        m_header.method |= static_cast<int>(value[12])<<10;
        m_header.method |= static_cast<int>(value[13])<<11;

        // Type
        m_header.messageClass = 0;
        m_header.messageClass |= static_cast<int>(value[4]);
        m_header.messageClass |= static_cast<int>(value[8]) <<1;
        m_state = HEADER_LENGTH;
        return SECTION_FINISH;
    }
}

int MessageParser::parseHeaderLength( MessageParserBuffer& buffer ) {
    Q_ASSERT(m_state == HEADER_LENGTH);
    bool ok;
    std::uint16_t val = buffer.NextUint16(&ok);
    if(!ok) {
      return IN_PROGRESS;
    }
    m_header.length = static_cast<std::size_t>(val);
    // Here we do a simple testing for the message length since based on the RCF
    // the message length must contains the bytes with padding which results in
    // the last 2 bits of the message length should always be zero .
    static const std::size_t kLengthMask = 3;
    if( (m_header.length & kLengthMask)  != 0 ) {
        // We don't understand such header since the last 2 bits of the length
        // is not zero zero. This is another way to tell if a packet is STUN or not
        return FAILED;
    }
    m_state = HEADER_MAGIC_ID;
    return SECTION_FINISH;
}

int MessageParser::parseHeaderMagicCookie( MessageParserBuffer& buffer ) {
    Q_ASSERT(m_state == HEADER_MAGIC_ID);
    bool ok;
    std::uint32_t magic_id;
    magic_id = buffer.NextUint32(&ok);
    if(!ok) {
      return IN_PROGRESS;
    }
    if( MAGIC_COOKIE != magic_id ) 
      return FAILED;
    else {
        m_state = HEADER_TRANSACTION_ID;
        return SECTION_FINISH;
    }
}

int MessageParser::parseHeaderTransactionID( MessageParserBuffer& buffer ) {
    Q_ASSERT(m_state == HEADER_TRANSACTION_ID );
    bool ok;
    buffer.NextBytes( m_header.transactionId.data(), m_header.transactionId.size(), &ok );
    if(!ok) {
      return IN_PROGRESS;
    }
    // We are finished here so we should populate the Message object right now
    // Set left message length for parsing attributes 
    m_leftMessageLength = m_header.length;
    // Populate the header
    m_outputMessage->header.messageClass = static_cast<MessageClass>(m_header.messageClass);
    m_outputMessage->header.method = static_cast<int>(m_header.method);
    m_outputMessage->header.transactionId = m_header.transactionId;

    m_state = MORE_VALUE;
    return SECTION_FINISH;
}

int MessageParser::parseMoreValue( MessageParserBuffer& buffer ) {
    // We need this function to decide where our state machine gonna go.
    // If after we finished transaction id, we find out that the length
    // of our body is zero, simply means we are done here since no attributes
    // are associated with our body. 
    Q_ASSERT(m_state == MORE_VALUE);
    if( m_leftMessageLength == 0 ) {
        buffer.Clear();
        m_attribute.Clear();
        m_state = HEADER_INITIAL_AND_TYPE;
        return FINISH;
    } else {
        m_attribute.Clear();
        m_state = ATTRIBUTE_TYPE;
        return SECTION_FINISH;
    }
}

int MessageParser::parseAttributeType( MessageParserBuffer& buffer ) {
    Q_ASSERT(m_state == ATTRIBUTE_TYPE);
    bool ok;
    m_attribute.type = buffer.NextUint16(&ok);
    if(!ok) {
      return IN_PROGRESS;
    }
    m_state = ATTRIBUTE_LENGTH;
    return SECTION_FINISH;
}

int MessageParser::parseAttributeLength( MessageParserBuffer& buffer ) {
    Q_ASSERT(m_state == ATTRIBUTE_LENGTH);
    bool ok;
    m_attribute.length = buffer.NextUint16(&ok);
    if(!ok) {
      return IN_PROGRESS;
    }
    m_state = ATTRIBUTE_VALUE;
    return SECTION_FINISH;
}

int MessageParser::parseAttributeValueNotAdd( MessageParserBuffer& buffer ) {
    bool ok;
    std::size_t padding_length = calculatePaddingSize(m_attribute.length);
    m_attribute.value.resize( static_cast<int>(padding_length) );
    buffer.NextBytes(m_attribute.value.data(),padding_length,&ok);
    if( !ok ) 
      return IN_PROGRESS;
    // Modify the left message length field : total size 
    // The value length + 2 bytes type + 2 bytes length 
    m_leftMessageLength -= 4 + padding_length;
    return SECTION_FINISH;
}

int MessageParser::parseAttributeValue( MessageParserBuffer& buffer ) {
    Q_ASSERT(m_state == ATTRIBUTE_VALUE);
    int ret = parseAttributeValueNotAdd(buffer);
    if( ret != SECTION_FINISH ) return ret;
    Attribute* attr = parseValue();
    if( attr == NULL ) return FAILED;
    m_outputMessage->attributes.emplace( attr->type(), std::unique_ptr<Attribute>(attr) );
    switch( attr->type() ) {
    case attrs::FINGER_PRINT:
        m_state = END_FINGERPRINT;
        break;
    case attrs::MESSAGE_INTEGRITY:
        m_state = END_MESSAGE_INTEGRITY;
        break;
    default:
        m_state = MORE_VALUE;
    }
    return SECTION_FINISH;
}

int MessageParser::parseEndWithFingerprint( MessageParserBuffer& buffer ) {
    // Finger print MUST BE the last message, if we meet more message than we
    // expected, we just return error that we cannot handle this message now
    Q_UNUSED(buffer);
    if( m_leftMessageLength != 0 )
        return FAILED;
    m_state = HEADER_INITIAL_AND_TYPE;
    return FINISH;
}

int MessageParser::parseEndMessageIntegrity( MessageParserBuffer& buffer ) {
    Q_UNUSED(buffer);
    if( m_leftMessageLength != 0 ) {
        m_state = ATTRIBUTE_ONLY_ALLOW_FINGERPRINT_TYPE;
        return IN_PROGRESS;
    } else {
        m_state = HEADER_INITIAL_AND_TYPE;
        return FINISH;
    }
}

nx_api::ParserState::Type MessageParser::parse( const nx::Buffer& user_buffer , std::size_t* bytes_transferred ) {
    Q_ASSERT( !user_buffer.isEmpty() );
    // Setting up the buffer environment variables
    MessageParserBuffer buffer(&m_tempBuffer,user_buffer);
    // Tick the parsing state machine
    do {
        int ret = 0;                                    
        switch(m_state)
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
                return nx_api::ParserState::failed;
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

} // namespase stun
} // namespase nx
