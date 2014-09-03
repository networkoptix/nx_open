#include "base_protocol_message_types.h"
#include <bitset>
#include <QtEndian>

// ============================================================
// Implementation for parser
// ============================================================

namespace nx_api {

QString Message::dump() const {
    QString ret;
    QTextStream stream(&ret);
    stream << QLatin1String("Message:\n");
    if( empty_ ) {
        stream<<QLatin1String("{<empty>}");
        return ret;
    }
    stream << QLatin1String(" Method:")<<message_method_<<QLatin1String("\n");
    stream << QLatin1String(" Class:");
    switch( message_class_ ) {
    case messageClassRequest:
        stream<<QLatin1String("Request\n"); break;
    case messageClassIndication:
        stream<<QLatin1String("Indication\n"); break;
    case messageClassErrorResponse:
        stream<<QLatin1String("ErrorResponse\n"); break;
    case messageClassSuccessResponse:
        stream<<QLatin1String("SuccessResponse\n"); break;
    default: break;
    }
    stream<< QLatin1String(" Attributes List:\n {");
    for( std::size_t i = 0 ; i < attributes_.size() ; ++i ) {
        stream
            <<QLatin1String("\n  [\n   Type:")<<attributes_[i].getType()
            <<QLatin1String("\n   Length:")<<attributes_[i].getValue().size()
            <<QLatin1String("\n   Value:")<<attributes_[i].getValue()<<QLatin1String("\n  ];");
    }
    stream<<"\n }\n";
    return ret;
}

namespace detail{

// The parser itself will nearly not consume any memory , if we have any buffer we will transfer the ownership
// using std::move to other object. In order to achieve partial parsing, internally a state machine is maintained
// as long as the user knows that he may not touch the buffer which is not processed by the MessageParser , the 
// consistent of the message is guaranteed. The parser itself will only stop at most by a double word. So literally
// speaking, the parser will return at most 3 bytes length which cannot be consumed based on the context information.
// in most cases, the parser will stay in a valid state that partially consumes all the data in the buffer .

class MessageParserImpl {
public:
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
        // Other state
        MORE_VALUE  
    };
    enum {
        IN_PROGRESS,
        DONE,
        MESSAGE_FINISH,
        FAILED
    };
    MessageParserImpl() : 
        output_message_(NULL),
        buffer_(NULL),
        position_(0),
        left_message_length_(0),
        state_(HEADER_INITIAL_AND_TYPE){}

    void SetMessage( Message* message ) {
        message->clear();
        output_message_ = message;
    }
    int Parse( const nx::Buffer* buffer , std::size_t* bytes_transferred );
    void Reset() {
        state_ = HEADER_INITIAL_AND_TYPE;
    }

private:
    // LAP implementation
    int ParseHeader_InitialAndType( std::size_t* consumed_bytes );
    int ParseHeader_Length( std::size_t* consumed_bytes );
    int ParseHeader_MagicId( std::size_t* consumed_bytes );
    int ParseHeader_TransactionId( std::size_t* consumed_bytes );
    int ParseAttribute_Type( std::size_t* consumed_bytes );
    int ParseAttribute_Length( std::size_t* consumed_bytes );
    int ParseAttribute_Value( std::size_t* consumed_bytes );
    int ParseMoreValue( std::size_t* consumed_bytes );
private:
    // Helper function 
    bool Ensure( std::size_t byte_size ) const {
        if( position_ + byte_size < buffer_->size() ) 
            return true;
        else 
            return false;
    }
    quint16 NextUint16( bool* ok );
    quint32 NextUint32( bool* ok );
    quint8 NextByte( bool* ok );
    // After calling each parsing attribute value, call this function to
    // pad the cursor to the nearest multiplier of 4 
    std::size_t CalcPaddingSize( std::size_t value_bytes );

private:

    struct STUNHeader {
        int message_class;
        int message_method;
        std::size_t message_length;
        Message::TransactionId transaction_id;
    };

    struct STUNAttr {
        quint16 length;
        nx::Buffer value;
        quint16 type;
        void clear() {
            length = 0;
            value.clear();
            type = 0;
        }
    };

    STUNHeader header_;
    STUNAttr attribute_;
    std::size_t left_message_length_;
    int state_;
    Message* output_message_;
    const nx::Buffer* buffer_;
    std::size_t position_;
};

quint16 MessageParserImpl::NextUint16( bool* ok ) {
    if( !Ensure(sizeof(quint16)) ) {
        *ok = false; return 0;
    } 
    quint16 ret = *reinterpret_cast<const quint16*>( buffer_->constData() + position_ );
    position_ += sizeof(quint16);
    *ok = true;
    return qFromBigEndian(ret);
}

quint32 MessageParserImpl::NextUint32( bool* ok ) {
    if( !Ensure(sizeof(quint32)) ) {
        *ok = false; return 0;
    }
    quint32 ret = *reinterpret_cast<const quint32*>( buffer_->constData() + position_ );
    position_ += sizeof(quint32);
    *ok = true;
    return qFromBigEndian(ret);
}

quint8 MessageParserImpl::NextByte( bool* ok ) {
    if( !Ensure(sizeof(quint8)) ) {
        *ok = false; return 0;
    }
    quint8 ret = *reinterpret_cast<const quint8*>( buffer_->constData() + position_ );
    position_ += sizeof(quint8);
    *ok = true;
    return ret;
}

std::size_t MessageParserImpl::CalcPaddingSize( std::size_t value_bytes ) {
    // STUN RFC indicates that the length for the attribute is the length
    // prior to padding. So we still need to calculate the padding by ourself.
    static const std::size_t kAlignMask = 3;
    return (value_bytes + kAlignMask) & ~kAlignMask;
}

#define CHECK &ok); if(!ok) { return IN_PROGRESS; } (void*)(NULL

int MessageParserImpl::ParseHeader_InitialAndType( std::size_t* consumed_bytes ) {
    Q_ASSERT(state_ == HEADER_INITIAL_AND_TYPE);
    bool ok;
    std::bitset<16> value = NextUint16(CHECK);
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
    quint16 val = NextUint16(CHECK);
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
    static const quint32 kMagicId = 0x2112A442;
    Q_ASSERT(state_ == HEADER_MAGIC_ID);
    bool ok;
    quint32 magic_id = NextUint32(CHECK);
    if( kMagicId != magic_id ) return FAILED;
    else {
        state_ = HEADER_TRANSACTION_ID;
        *consumed_bytes = 4;
        return DONE;
    }
}

int MessageParserImpl::ParseHeader_TransactionId( std::size_t* consumed_bytes ) {
    Q_ASSERT(state_ == HEADER_TRANSACTION_ID);
    bool ok;
    std::size_t pos = 0;
    do {
        quint8 byte = NextByte(CHECK);
        header_.transaction_id.at(pos) = byte;
    } while( pos != Message::kTransactionIdLength );

    *consumed_bytes = Message::kTransactionIdLength;
    // We are finished here so we should populate the Message object right now
    output_message_->init( header_.message_class , header_.message_method , header_.transaction_id );
    // Set left message length for parsing attributes 
    left_message_length_ = header_.message_length;
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
        return MESSAGE_FINISH;
    } else {
        attribute_.clear();
        state_ = ATTRIBUTE_TYPE;
        return DONE;
    }
}

int MessageParserImpl::ParseAttribute_Type( std::size_t* consumed_bytes ) {
    Q_ASSERT(state_ == ATTRIBUTE_TYPE);
    bool ok;
    attribute_.type = NextUint16(CHECK);
    *consumed_bytes = 2;
    state_ = ATTRIBUTE_LENGTH;
    return DONE;
}

int MessageParserImpl::ParseAttribute_Length( std::size_t* consumed_bytes ) {
    Q_ASSERT(state_ == ATTRIBUTE_LENGTH);
    bool ok;
    attribute_.length = NextUint16(CHECK);
    *consumed_bytes = 2;
    state_ = ATTRIBUTE_VALUE;
    return DONE;
}

int MessageParserImpl::ParseAttribute_Value( std::size_t* consumed_bytes ) {
    Q_ASSERT(state_ == ATTRIBUTE_VALUE);
    bool ok;
    std::size_t b = static_cast<std::size_t>(attribute_.value.size());
    attribute_.value.reserve( attribute_.length );
    do {
        quint8 byte = NextByte(&ok);
        if(!ok) {
            *consumed_bytes = b;
            return IN_PROGRESS;
        }
        attribute_.value.push_back(byte);
    } while( b < static_cast<std::size_t>(attribute_.length) );

    // Once we have reached here, we need to consider the padding bytes as well
    std::size_t padding_value_size = CalcPaddingSize(attribute_.value.size());
    // We only need the difference between before padding and after padding 
    *consumed_bytes += attribute_.value.size() - padding_value_size;
    // Modify the left message length field : total size 
    // The value length + 2 bytes type + 2 bytes length 
    left_message_length_ -= 4 + padding_value_size;
    // Add the parsed value into the buffer of value
    output_message_->addAttribute( Message::Attribute(attribute_.type,std::move(attribute_.value)) );

    state_ = MORE_VALUE;
    return DONE;
}

#undef CHECK
 

// Save me some typings for each ParseXXX function 

#define INVOKE(x) \
    do { \
    std::size_t b; \
    int ret = x(&b); \
    switch(ret) { \
    case DONE: \
    *bytes_transferred += b; position_ += b ; break; \
    case IN_PROGRESS: \
    return ParserState::inProgress; \
    case FAILED: \
    return ParserState::failed; \
    case MESSAGE_FINISH: \
    return ParserState::done; \
    default : Q_ASSERT(0); break; }}while(0); break

int MessageParserImpl::Parse( const nx::Buffer* buffer , std::size_t* bytes_transferred ) {
    // Setting up the buffer environment variables
    buffer_ = buffer;
    position_ = 0;
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
        case MORE_VALUE:
            INVOKE(ParseMoreValue);
        default: Q_ASSERT(0); return 0;
        }
    } while( true );
}

#undef INVOKE

MessageParserImplPtr::MessageParserImplPtr() : std::unique_ptr< MessageParserImpl > (){}
MessageParserImplPtr::MessageParserImplPtr( MessageParserImpl* impl ) : std::unique_ptr<MessageParserImpl>(impl){}
MessageParserImplPtr::~MessageParserImplPtr(){}

}// namespace detail

MessageParser::MessageParser() : impl_( new detail::MessageParserImpl() ) {}

void MessageParser::setMessage( Message* const msg ) {
    impl_->SetMessage(msg);
}

ParserState::Type MessageParser::parse( const nx::Buffer& buf, size_t* bytesProcessed ) {
    return static_cast<ParserState::Type>(impl_->Parse(&buf,bytesProcessed));
}

void MessageParser::reset() {
    impl_->Reset();
}

MessageParser::~MessageParser() {
}

}// namespace nx_api

#if 0
// =================================================
// Implementation for serializer
// =================================================

namespace nx_api{
namespace detail{

class MessageSerializerImpl {
public:
    void SetMessage( const Message& message ) {
        message_ = &message;
    }

    SerializerState::Type Serialize( const nx::Buffer* buffer, const size_t* bytes_produced );

private:
    // For header length serialization , it is a little bit tricky since we may not
    // be able to calculate the message length after we finished the serialization( We can , but
    // we don't want to , another pass for the message ). So we return a pointer for the 
    // user to assign the length of the message later on.
    quint16* SerializeHeader();
    void SerializeHeader_InitialAndType();
    quint16* SerializeHeader_MessageLength();
    void SerializeMagicIdAndTransactionId();

    void SerializeBody();
    void SerializeTLV();



private:
    const Message* message_;
    std::size_t bytes_produced_;
};


}// namespace detail
}// namespace nx_api
#endif 
