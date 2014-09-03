#include "stun_message_parser.h"
#include "stun_message.h"
#include <QtEndian>
#include <QString>

#include <bitset>
#include <cstdint>
#include <deque>

namespace nx_stun{
namespace detail{
using namespace attr;

// We separate the value for attribute parsing into another class for the future modification of 
// adding new STUN attribute support. I haven't use common factory method to solve this problem 
// since we may not need runtime flexibility here.
namespace {

// Parsing for each specific type
Attribute* Value_ParseXORMappedAddress( const nx::Buffer& raw , int length , const Header& header ) {
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
        // ipv6
#define DO_(index,mask) \
    do { \
    xor_comp = qFromBigEndian(*reinterpret_cast<const std::uint16_t*>(raw.constData()+4+(index)*2)); \
    attribute->address.ipv6.array[index] = xor_comp ^ (mask); } while(0)

        // The RFC doesn't indicate how to concatenate, I just assume it with the natural byte order
        std::uint16_t xor_comp;
        DO_(0,kMagicCookieHigh);
        DO_(1,kMagicCookieLow);
        DO_(2,*reinterpret_cast<const std::uint16_t*>(header.transactionID.bytes));
        DO_(3,*reinterpret_cast<const std::uint16_t*>(header.transactionID.bytes+2));
        DO_(4,*reinterpret_cast<const std::uint16_t*>(header.transactionID.bytes+4));
        DO_(5,*reinterpret_cast<const std::uint16_t*>(header.transactionID.bytes+6));
        DO_(6,*reinterpret_cast<const std::uint16_t*>(header.transactionID.bytes+8));
        DO_(7,*reinterpret_cast<const std::uint16_t*>(header.transactionID.bytes+10));
#undef DO_
    }

    return attribute.release();
}


Attribute* Value_ParseErrorCode( const nx::Buffer& raw , int length , const Header& header ) {
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
    attribute->_class |= static_cast<int>( value[10]) << 10;
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

Attribute* Value_ParseFingerprint( const nx::Buffer& raw , int length , const Header& header ) {
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

Attribute* Value_ParseMessageIntegrity( const nx::Buffer& raw , int length , const Header& header ) {
    Q_UNUSED(header);
    if( raw.size() != MessageIntegrity::SHA1_HASH_SIZE )
        return NULL;
    std::unique_ptr<MessageIntegrity> attribute( new MessageIntegrity() );
    attribute->length = length;
    attribute->type = AttributeType::messageIntegrity;
    qCopy( raw.begin() , raw.end() , attribute->hmac.begin() );
    return attribute.release();
}

Attribute* Value_ParseUnknownAttribute( const nx::Buffer& raw , int length , const Header& header ) {
    Q_UNUSED(header);
    std::unique_ptr< UnknownAttribute > attribute( new UnknownAttribute() );
    attribute->length = length;
    attribute->type = AttributeType::unknown;
    attribute->value = raw;
    return attribute.release();
}

Attribute* Value_Parse( const nx::Buffer& raw , int type , int length , const Header& header ) {
    switch( type ) {
    case attr::AttributeType::xorMappedAddress: return Value_ParseXORMappedAddress(raw,length,header);
    case attr::AttributeType::errorCode: return Value_ParseErrorCode(raw,length,header);
    case attr::AttributeType::messageIntegrity: return Value_ParseMessageIntegrity(raw,length,header);
    case attr::AttributeType::fingerprint: return Value_ParseFingerprint(raw,length,header);
    default: return Value_ParseUnknownAttribute(raw,length,header);
    }
}

// This message parser buffer add a simple workaround with the original partial data buffer.
// It will consume all the data in the user buffer, but provide a more consistent interface
// for user. So if the parser stuck at one byte data, the MessageParserBuffer will temporary
// store that one byte and make user buffer drained. However without losing any single bytes

class MessageParserBuffer {
public:
    MessageParserBuffer(): 
        buffer_(NULL), 
        position_(0){ }
    std::uint16_t NextUint16( bool* ok );
    std::uint32_t NextUint32( bool* ok );
    std::uint8_t NextByte( bool* ok );
    void NextBytes( std::uint8_t* bytes , std::size_t sz , bool* ok );
    void Bind( const nx::Buffer* buffer ) {
        buffer_ = buffer;
        position_ = 0;
        // Do not modify the temp_buffer_size_ here
    }
    void Clear() {
        temp_buffer_.clear();
    }
private:
    bool Ensure( std::size_t byte_size , void* buffer );

    std::deque<char> temp_buffer_;
    const nx::Buffer* buffer_;
    std::size_t position_;
};

// Ensure will check the temporary buffer and also the buffer in nx::Buffer.
// Additionally, if ensure cannot ensure the buffer size, it will still drain
// the original buffer , so the user buffer will always be consumed over .
bool MessageParserBuffer::Ensure( std::size_t byte_size , void* buffer ) {
    // Get the available bytes that we can read from the stream
    std::size_t available_bytes = temp_buffer_.size() + buffer_->size() - position_;
    if( available_bytes < byte_size ) {
        // Drain the user buffer here
        for( std::size_t i = position_ ; i < buffer_->size() ; ++i )
            temp_buffer_.push_back(buffer_->at(static_cast<int>(i)));
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
                *(reinterpret_cast<char*>(buffer)) = buffer_->at(static_cast<int>(position_));
                position_ += 1; 
                return true;
            case 2:
                *(reinterpret_cast<std::uint16_t*>(buffer)) = 
                    *(reinterpret_cast<const std::uint16_t*>(buffer_->constData()+position_));
                position_ += 2;
                return true;
            case 4:
                *(reinterpret_cast<std::uint32_t*>(buffer)) = 
                    *(reinterpret_cast<const std::uint32_t*>(buffer_->constData()+position_));
                position_ += 4;
                return true;
            default: Q_ASSERT(0); return false;
            }
        } else {
            memcpy(reinterpret_cast<char*>(buffer)+pos,buffer_->constData() + position_, byte_size - pos );
            position_ += byte_size - pos;
            return true;
        }
    }
}

std::uint16_t MessageParserBuffer::NextUint16( bool* ok ) {
    std::uint16_t value;
    if( !Ensure(sizeof(quint16),&value) ) {
        *ok = false; return 0;
    } 
    *ok = true;
    return qFromBigEndian(value);
}

std::uint32_t MessageParserBuffer::NextUint32( bool* ok ) {
    std::uint32_t value;
    if( !Ensure(sizeof(quint32),&value) ) {
        *ok = false; return 0;
    }
    *ok = true;
    return qFromBigEndian(value);
}

std::uint8_t MessageParserBuffer::NextByte( bool* ok ) {
    std::uint8_t value;
    if( !Ensure(sizeof(quint8),&value) ) {
        *ok = false; return 0;
    }
    *ok = true;
    return value;
}

void MessageParserBuffer::NextBytes( std::uint8_t* bytes , std::size_t sz , bool* ok ) {
    if(!Ensure(sz,bytes)) {
        *ok = false;
        return;
    }
    *ok = true;
}

}// namespace

// The parser itself will nearly not consume any memory , if we have any buffer we will transfer the ownership
// using std::move to other object. In order to achieve partial parsing, internally a state machine is maintained
// as long as the user knows that he may not touch the buffer which is not processed by the MessageParser , the 
// consistent of the message is guaranteed. The parser itself will only stop at most by a double word. So literally
// speaking, the parser will return at most 3 bytes length which cannot be consumed based on the context information.
// in most cases, the parser will stay in a valid state that partially consumes all the data in the buffer .

class MessageParserImpl {
public:
    static const int kTransactionIdLength = 12;
    enum {
        // Header
        HEADER_INITIAL_AND_TYPE ,
        HEADER_LENGTH,
        HEADER_MAGIC_ID,
        HEADER_TRANSACTION_ID,
        // Attributes
        ATTRIBUTE_TYPE,
        ATTRIBUTE_LENGTH,
        ATTRIBUTE_VALUE,

        // Attributes for fingerprint only
        ATTRIBUTE_ONLY_ALLOW_FINGERPRINT_LENGTH,
        ATTRIBUTE_ONLY_ALLOW_FINGERPRINT_TYPE,
        ATTRIBUTE_ONLY_ALLOW_FINGERPRINT_VALUE,

        // Other states
        MORE_VALUE,
        END_MESSAGE_INTEGRITY,
        END_FINGERPRINT
    };

    enum {
        IN_PROGRESS,
        DONE,
        MESSAGE_FINISH,
        FAILED
    };

    MessageParserImpl() : 
        output_message_(NULL),
        left_message_length_(0),
        state_(HEADER_INITIAL_AND_TYPE){}

    void SetMessage( Message* message ) {
        message->clear();
        output_message_ = message;
    }
    int Parse( const nx::Buffer* buffer , std::size_t* bytes_transferred );
    void Reset() {
        state_ = HEADER_INITIAL_AND_TYPE;
        buffer_.Clear();
    }
    nx_api::ParserState::Type State() {
        return nx_api::ParserState::init;
    }
private:
    // LAP implementation
    int ParseHeader_InitialAndType( std::size_t* consumed_bytes );
    int ParseHeader_Length( std::size_t* consumed_bytes );
    int ParseHeader_MagicId( std::size_t* consumed_bytes );
    int ParseHeader_TransactionId( std::size_t* consumed_bytes );
    int ParseAttribute_Type( std::size_t* consumed_bytes );
    int ParseAttribute_Length( std::size_t* consumed_bytes );
    int ParseAttribute_Value_NotAdd( std::size_t* consumed_bytes );
    int ParseAttribute_Value( std::size_t* consumed_bytes );
    int ParseAttribute_FingerprintType( std::size_t* consumed_bytes ) {
        int ret = ParseAttribute_Type(consumed_bytes);
        state_ = ATTRIBUTE_ONLY_ALLOW_FINGERPRINT_LENGTH;
        return ret;
    }
    int ParseAttribute_FingerprintLength( std::size_t* consumed_bytes ) {
        int ret = ParseAttribute_Length(consumed_bytes);
        state_ = ATTRIBUTE_ONLY_ALLOW_FINGERPRINT_LENGTH;
        return ret;
    }
    int ParseAttribute_FinterprintValue( std::size_t* consumed_bytes ) {
        int ret = ParseAttribute_Value_NotAdd( consumed_bytes );
        if( ret != DONE ) return ret;
        else {
            // Based on RFC , it says we MUST ignore the message following by message
            // integrity only fingerprint will need to handle. And we just check the
            // fingerprint message followed by the STUN packet. If we have found one
            // our parser state converts to a END_FINGERPINT states
            if( attribute_.type == static_cast<int>(AttributeType::fingerprint) ) {
                state_ = END_FINGERPRINT;
            } else {
                state_ = MORE_VALUE;
            }
            return DONE;
        }
    }
    int ParseMoreValue( std::size_t* consumed_bytes );
    // 2 possible ending states
    int ParseEndMessageIntegrity( std::size_t* consumed_bytes );
    int ParseEndWithFingerprint( std::size_t* consumed_bytes );
    std::size_t CalcPaddingSize( std::size_t value_bytes ) {
        // STUN RFC indicates that the length for the attribute is the length
        // prior to padding. So we still need to calculate the padding by ourself.
        static const std::size_t kAlignMask = 3;
        return (value_bytes + kAlignMask) & ~kAlignMask;
    }
private:

    struct STUNHeader {
        int message_class;
        int message_method;
        std::size_t message_length;
        TransactionID transaction_id;
    };

    struct STUNAttr {
        std::uint16_t length;
        nx::Buffer value;
        std::uint16_t type;
        void Clear() {
            length = 0;
            value.clear();
            type = 0;
        }
    };

    STUNHeader header_;
    STUNAttr attribute_;
    Message* output_message_;
    MessageParserBuffer buffer_;
    std::size_t left_message_length_;
    int state_;
};

#define CHECK &ok); if(!ok) { return IN_PROGRESS; } (void*)(NULL

int MessageParserImpl::ParseHeader_InitialAndType( std::size_t* consumed_bytes ) {
    Q_ASSERT(state_ == HEADER_INITIAL_AND_TYPE);
    bool ok;
    std::bitset<16> value = buffer_.NextUint16(CHECK);
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
        // We are done here
        *consumed_bytes = 2;
        state_ = HEADER_LENGTH;
        return DONE;
    }
}

int MessageParserImpl::ParseHeader_Length( std::size_t* consumed_bytes ) {
    Q_ASSERT(state_ == HEADER_LENGTH);
    bool ok;
    std::uint16_t val = buffer_.NextUint16(CHECK);
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
    *consumed_bytes = 2;
    return DONE;
}

int MessageParserImpl::ParseHeader_MagicId( std::size_t* consumed_bytes ) {
    Q_ASSERT(state_ == HEADER_MAGIC_ID);
    bool ok;
    std::uint32_t magic_id;
    buffer_.NextBytes(reinterpret_cast<std::uint8_t*>(&magic_id),
        sizeof(std::uint32_t),CHECK);
    if( MAGIC_COOKIE != magic_id ) return FAILED;
    else {
        state_ = HEADER_TRANSACTION_ID;
        *consumed_bytes = 4;
        return DONE;
    }
}

int MessageParserImpl::ParseHeader_TransactionId( std::size_t* consumed_bytes ) {
    Q_ASSERT(state_ == HEADER_TRANSACTION_ID );
    bool ok;
    buffer_.NextBytes( static_cast<std::uint8_t*>(header_.transaction_id.bytes) , kTransactionIdLength , CHECK);
    *consumed_bytes = kTransactionIdLength;
    // We are finished here so we should populate the Message object right now
    // Set left message length for parsing attributes 
    left_message_length_ = header_.message_length;
    // Populate the header
    output_message_->header.messageClass = static_cast<MessageClass>(header_.message_class);
    output_message_->header.messageLength= static_cast<int>(header_.message_length);
    output_message_->header.method = static_cast<int>(header_.message_method);
    output_message_->header.transactionID = header_.transaction_id;

    state_ = MORE_VALUE;
    return DONE;
}

int MessageParserImpl::ParseMoreValue( std::size_t* consumed_bytes ) {
    // We need this function to decide where our state machine gonna go.
    // If after we finished transaction id, we find out that the length
    // of our body is zero, simply means we are done here since no attributes
    // are associated with our body. 
    Q_ASSERT(state_ == MORE_VALUE);
    *consumed_bytes = 0;
    if( left_message_length_ == 0 ) {
        buffer_.Clear();
        attribute_.Clear();
        return MESSAGE_FINISH;
    } else {
        attribute_.Clear();
        state_ = ATTRIBUTE_TYPE;
        return DONE;
    }
}

int MessageParserImpl::ParseAttribute_Type( std::size_t* consumed_bytes ) {
    Q_ASSERT(state_ == ATTRIBUTE_TYPE);
    bool ok;
    attribute_.type = buffer_.NextUint16(CHECK);
    *consumed_bytes = 2;
    state_ = ATTRIBUTE_LENGTH;
    return DONE;
}

int MessageParserImpl::ParseAttribute_Length( std::size_t* consumed_bytes ) {
    Q_ASSERT(state_ == ATTRIBUTE_LENGTH);
    bool ok;
    attribute_.length = buffer_.NextUint16(CHECK);
    *consumed_bytes = 2;
    state_ = ATTRIBUTE_VALUE;
    return DONE;
}

int MessageParserImpl::ParseAttribute_Value_NotAdd( std::size_t* consumed_bytes ) {
    bool ok;
    std::size_t padding_length = CalcPaddingSize(attribute_.length);
    attribute_.value.resize( static_cast<int>(padding_length) );
    buffer_.NextBytes(reinterpret_cast<std::uint8_t*>(attribute_.value.data()),padding_length,&ok);
    if( !ok ) return IN_PROGRESS;
    // We only need the difference between before padding and after padding 
    *consumed_bytes += attribute_.value.size() - padding_length;
    // Modify the left message length field : total size 
    // The value length + 2 bytes type + 2 bytes length 
    left_message_length_ -= 4 + padding_length;
    return DONE;
}

int MessageParserImpl::ParseAttribute_Value( std::size_t* consumed_bytes ) {
    Q_ASSERT(state_ == ATTRIBUTE_VALUE);
    int ret = ParseAttribute_Value_NotAdd(consumed_bytes);
    if( ret != DONE ) return ret;
    Attribute* attr = Value_Parse( attribute_.value , attribute_.type , attribute_.length , output_message_->header );
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
    return DONE;
}

int MessageParserImpl::ParseEndWithFingerprint( std::size_t* consumed_bytes ) {
    // Finger print MUST BE the last message, if we meet more message than we
    // expected, we just return error that we cannot handle this message now
    *consumed_bytes = 0;
    if( left_message_length_ != 0 )
        return FAILED;
    return MESSAGE_FINISH;
}

int MessageParserImpl::ParseEndMessageIntegrity( std::size_t* consumed_bytes ) {
    *consumed_bytes = 0;
    if( left_message_length_ != 0 ) {
        state_ = ATTRIBUTE_ONLY_ALLOW_FINGERPRINT_TYPE;
        return IN_PROGRESS;
    } else {
        return MESSAGE_FINISH;
    }
}

#undef CHECK

// Save me some typings for each ParseXXX function 

#define INVOKE(x) \
    do { \
    std::size_t b; \
    int ret = x(&b); \
    switch(ret) { \
    case DONE: \
    *bytes_transferred += b; break; \
    case IN_PROGRESS: \
    return nx_api::ParserState::inProgress; \
    case FAILED: \
    return nx_api::ParserState::failed; \
    case MESSAGE_FINISH: \
    return nx_api::ParserState::done; \
    default : Q_ASSERT(0); break; }}while(0); break

int MessageParserImpl::Parse( const nx::Buffer* buffer , std::size_t* bytes_transferred ) {
    Q_ASSERT( !buffer->isEmpty() );
    // Setting up the buffer environment variables
    buffer_.Bind(buffer);
    // We will drain the buffer, no matter how many data it contains
    *bytes_transferred = buffer->size();
    // Tick the parsing state machine
    do {
        switch(state_) {
        case HEADER_INITIAL_AND_TYPE:
            INVOKE(ParseHeader_InitialAndType);
        case HEADER_LENGTH:
            INVOKE(ParseHeader_Length);
        case HEADER_MAGIC_ID:
            INVOKE(ParseHeader_MagicId);
        case HEADER_TRANSACTION_ID:
            INVOKE(ParseHeader_TransactionId);
        case ATTRIBUTE_TYPE:
            INVOKE(ParseAttribute_Type);
        case ATTRIBUTE_LENGTH:
            INVOKE(ParseAttribute_Length);
        case ATTRIBUTE_VALUE:
            INVOKE(ParseAttribute_Value);
        case ATTRIBUTE_ONLY_ALLOW_FINGERPRINT_TYPE:
            INVOKE(ParseAttribute_FingerprintType);
        case ATTRIBUTE_ONLY_ALLOW_FINGERPRINT_LENGTH:
            INVOKE(ParseAttribute_FingerprintLength);
        case ATTRIBUTE_ONLY_ALLOW_FINGERPRINT_VALUE:
            INVOKE(ParseAttribute_FinterprintValue);
        case MORE_VALUE:
            INVOKE(ParseMoreValue);
        case END_FINGERPRINT:
            INVOKE(ParseEndWithFingerprint);
        case END_MESSAGE_INTEGRITY:
            INVOKE(ParseEndMessageIntegrity);
        default: Q_ASSERT(0); return 0;
        }
    } while( true );
}

#undef INVOKE

MessageParserImplPtr::MessageParserImplPtr( MessageParserImpl* ptr ) :
    std::unique_ptr<MessageParserImpl>(ptr){}

MessageParserImplPtr::~MessageParserImplPtr(){}

}// namespace detail

MessageParser::MessageParser():impl_( new detail::MessageParserImpl() ) {}

MessageParser::~MessageParser(){}

nx_api::ParserState::Type MessageParser::state() const {
    return impl_->State();
}

nx_api::ParserState::Type MessageParser::parse( const nx::Buffer& buf , size_t* bytes_processed ) {
    return impl_->Parse(&buf,bytes_processed);
}

void MessageParser::setMessage( Message* const msg ) {
    impl_->SetMessage(msg);
}

void MessageParser::reset() {
    impl_->Reset();
}

}// namespace nx_stun
