/**********************************************************
* 23 dec 2013
* a.kolesnikov
***********************************************************/

#ifndef BASE_STREAM_MESSAGE_TYPES_H
#define BASE_STREAM_MESSAGE_TYPES_H

#include <utils/network/buffer.h>
#include <array>
#include <utility>
#include <vector>
#include <memory>

namespace nx_api
{
    namespace ParserState
    {
        typedef int Type;

        static const int init = 1;
        static const int inProgress = 2;
        static const int done = 3;
        static const int failed = 4;
    };

    namespace SerializerState
    {
        typedef int Type;

        static const int needMoreBufferSpace = 1;
        static const int done = 2;
    };

    // A message represents a specific STUN package that is good to go. The general usage is like this:
    // Message message(class,method,SomeTransactionId);
    // message<<Message::Attribute(SomeType,SomeValue);
    // message<<Message::Attribute(SomeOtherType,SomeOtherValue);
    // 2) Or you could have an empty message like this:
    // Message message;
    // message.init(class,method,SomeTransactionId); // this initialize the message
    // message<<Message::Attribute(SomeType,SomeValue); // add a new attribute to that message

    class Message
    {
    public:
        static const std::size_t kTransactionIdLength = 12;
        typedef std::array<char,kTransactionIdLength> TransactionId;

        enum {
            messageClassRequest  = 0 ,  // 00
            messageClassIndication= 1 , // 01 
            messageClassSuccessResponse = 2  , // 10
            messageClassErrorResponse = 3 // 11
        };

        Message() : empty_(true) {}
        Message( int message_class , int message_method , const TransactionId& transaction_id ) :
            transaction_id_(transaction_id),empty_(false){
                setMessageClassAndMethod(message_class,message_method);
        }
        void clear() {
            empty_ = true;
        }
        // transaction Id
        const TransactionId& getMessageTransactionId() const {
            Q_ASSERT(!isEmpty());
            return transaction_id_;
        }
        // message type/method
        int getMessageClass() const {
            return message_class_;
        }
        int getMessageMethod() const {
            return message_method_;
        }
        // init the message
        void init( int message_class , int message_method , const std::array<char,12>& transaction_id ) {
            setMessageClassAndMethod(message_class,message_method);
            transaction_id_=transaction_id;
            attributes_.clear();
            empty_ = false;
        }
        // This class will only store decoded type which means is good for using and printing.
        class Attribute {
        public:
            Attribute( quint16 type , const nx::Buffer& value ) :
                type_(type),value_(value),empty_(false){}
            Attribute( quint16 type , nx::Buffer&& value ) :
                type_(type),value_(std::move(value)),empty_(false){}
            Attribute() : empty_(true) {}
            Attribute( Attribute&& attribute ) {
                if( !attribute.empty_ ) {
                    type_ = attribute.type_;
                    value_ = std::move(attribute.value_);
                    empty_ = false;
                } else {
                    empty_ = true;
                }
            }
            Attribute& operator = ( Attribute&& attribute ) {
                if( this == &attribute ) return *this;
                if( !attribute.empty_ ) {
                    type_ = attribute.type_;
                    value_ = std::move(attribute.value_);
                    empty_ = false;
                } else {
                    empty_ = true;
                }
                return *this;
            }
            void init( int type , const nx::Buffer& buffer ) {
                empty_ = false;
                type_ = type;
                value_= buffer;
            }
            void init( int type , nx::Buffer&& buffer ) {
                empty_ = false;
                type_ = type;
                value_= std::move(buffer);
            }
            const nx::Buffer& getValue() const {
                Q_ASSERT(!isEmpty());
                return value_;
            }
            quint16 getType() const {
                Q_ASSERT(!isEmpty());
                return type_;
            }
            void setType( int type ) {
                Q_ASSERT(!isEmpty());
                type_ = type;
            }
            void setValue( const nx::Buffer& value ) {
                Q_ASSERT(!isEmpty());
                value_ = value;
            }
            bool isComprehensionRequired() const {
                Q_ASSERT(!isEmpty());
                return type_ <= 0x7FFF;
            }
            bool isEmpty() const {
                return empty_;
            }
        private:
            quint16 type_;
            nx::Buffer value_;
            bool empty_;
        };
        // attribute accessor
        void addAttribute( const Attribute& attribute ) {
            Q_ASSERT(!isEmpty());
            attributes_.push_back(attribute);
        }
        void addAttribute( Attribute&& attribute ) {
            Q_ASSERT(!isEmpty());
            attributes_.push_back( std::move(attribute) );
        }
        Attribute* addAttribute() {
            Q_ASSERT(!isEmpty());
            return &(*attributes_.emplace(attributes_.end()));
        }
        const Attribute& getAttribute( std::size_t index ) const {
            Q_ASSERT(!isEmpty());
            return attributes_[index];
        }
        Attribute& getAttribute( std::size_t index ) {
            Q_ASSERT(!isEmpty());
            return attributes_[index];
        }
        std::size_t getAttributeSize() const {
            Q_ASSERT(!isEmpty());
            return attributes_.size();
        }
        void clearAttributes() {
            Q_ASSERT(!isEmpty());
            attributes_.clear();
        }
        Message& operator << ( const Attribute& attribute ) {
            addAttribute(attribute); 
            return *this;
        }
        Message& operator << ( Attribute&& attribute ) {
            addAttribute(attribute);
            return *this;
        }
        // checking if the message is empty or not
        bool isEmpty() const {
            return empty_;
        }

        // This function will dump the internal state of the Message object
        // and it can be used to do debugging for the message object .
        QString dump() const;
    private:
        bool isValidMessageClass( int cls ) const {
            // only two bits is allowed here
            return cls >=0 && cls <= 3;
        }
        bool isValidMessageMethod  ( int method ) {
            // a message type can be up to 12 bits which is less than 2 << 12
            return method >= 0 && method < 2 << 12;
        }
        void setMessageClassAndMethod( int message_type , int message_method ) {
            Q_ASSERT(isValidMessageClass(message_type));
            Q_ASSERT(isValidMessageMethod(message_method));
            message_class_ = message_type;
            message_method_= message_method;
        }
    private:
        std::vector<Attribute> attributes_;
        TransactionId transaction_id_;
        int message_class_;
        int message_method_;
        bool empty_;
    };

    // pimpl
    namespace detail {
        class MessageParserImpl;
        class MessageSerializerImpl;
        struct MessageParserImplPtr : public std::unique_ptr<MessageParserImpl> {
            MessageParserImplPtr();
            MessageParserImplPtr( MessageParserImpl* );
            ~MessageParserImplPtr();
        };
        struct MessageSerializerImplPtr : public std::unique_ptr<MessageParserImplPtr> {
            MessageSerializerImplPtr();
            MessageSerializerImplPtr( MessageParserImplPtr* );
            ~MessageSerializerImplPtr();
        };
    }

    //!Demonstrates API of message parser
    class MessageParser
    {
    public:
        MessageParser();
        ~MessageParser();
        //!Set message object to parse to
        void setMessage( Message* const msg );
        //!Returns current parse state
        /*!
            Methods returns if:\n
                - end of message found
                - source data depleted

            \param buf
            \param bytesProcessed Number of bytes from \a buf which were read and parsed is stored here
            \note \a *buf MAY NOT contain whole message, but any part of it (it can be as little as 1 byte)
            \note Reads whole message even if parse error occured
        */
        ParserState::Type parse( const nx::Buffer& /*buf*/, size_t* /*bytesProcessed*/ );

        //!Resets parse state and prepares for parsing different data
        void reset();
    private:
        detail::MessageParserImplPtr impl_;
    };

    //!Demonstrates API of message serializer
    class MessageSerializer
    {
    public:
        //!Set message to serialize
        void setMessage( Message&& message );

        SerializerState::Type serialize( nx::Buffer* const buffer, size_t* bytesWritten );
    private:
        detail::MessageSerializerImplPtr impl_;
    };
}

#endif  //BASE_STREAM_MESSAGE_TYPES_H
