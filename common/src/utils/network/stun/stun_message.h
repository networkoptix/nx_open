/**********************************************************
* 18 dec 2013
* a.kolesnikov
***********************************************************/

#ifndef STUN_MESSAGE_H
#define STUN_MESSAGE_H

#include <cstdint>
#include <utils/network/buffer.h>
#include <memory>
#include <unordered_map>
#include <vector>
#include <array>

#include <utils/common/cpp14.h>

//!Implementation of STUN protocol (rfc5389)
namespace nx_stun
{
    static const std::uint32_t MAGIC_COOKIE = 0x2112A442;
    // Four components for MAGIC_COOKIE which is used for parsing/serialization of XorMappedAddress
    static const std::uint16_t MAGIC_COOKIE_HIGH = static_cast<std::uint16_t>( MAGIC_COOKIE >> 16 );
    static const std::uint16_t MAGIC_COOKIE_LOW  = static_cast<std::uint16_t>( MAGIC_COOKIE & 0x0000ffff );
    // For STUN crc32 calculation 
    // 0x5354554e;
    static const std::uint32_t STUN_FINGERPRINT_XORMASK[4] = {
        0x0000004e,
        0x00005500,
        0x00540000,
        0x53000000
    };
    //96-bit transaction ID
    struct TransactionID
    {
        static const std::size_t TRANSACTION_ID_LENGTH = 12;
        char bytes[TRANSACTION_ID_LENGTH];

        // Using union to pack struct is behavior undefined
        // here, since we cannot rely on 1) layout of structure,
        // 2) alignment cross complier. Default behavior is 
        // layout to 8 which doesn't work with our case for 12 bytes.
        // If user want to use high/low to get the transaction ID,
        // using this function to explicitly grab the representational
        // numeric value. 
        void get( std::uint32_t* high , std::uint64_t* low ) const {
            *high = *reinterpret_cast<const std::uint32_t*>(bytes + sizeof(*low));
            *low = *reinterpret_cast<const std::uint64_t*>(bytes);
        }
        void set( std::uint32_t high , std::uint64_t low ) {
            memcpy(bytes,&low,sizeof(low));
            memcpy(bytes+sizeof(low),&high,sizeof(high));
        }

        TransactionID( std::uint32_t high = 0, std::uint64_t low = 0 ) {
            set( high, low );
        }
        static TransactionID generateNew() {
            TransactionID id;
            id.set( static_cast< std::uint32_t >( qrand() ),
                    static_cast< std::uint64_t >( qrand() ) +
                    ( static_cast< std::uint64_t >( qrand() ) << 32 ) );
            return id;
        }
    };

    enum class MessageClass
    {
        request = 0,
        indication,
        successResponse,
        errorResponse
    };

    //!Contains STUN method types defined in RFC
    enum class MethodType
    {
        binding = 1,
        //!Starting value for custom STUN methods
        userMethod = 2
    };

    class Header
    {
    public:
        MessageClass messageClass;
        int method;
        TransactionID transactionID;

        Header( MessageClass messageClass_ = MessageClass::request,
                int method_ = 0,
                TransactionID transactionID_ = TransactionID() );
    };

    namespace ErrorCode
    {
        typedef int Type;

        static const int tryAlternate = 300;
        static const int badRequest = 400;
        static const int unauthorized = 401;
        static const int notFound = 404;
        static const int unknownAttribute = 420;
        static const int staleNonce = 438;
        static const int serverError = 500;
    }

    //!Contains STUN attributes
    namespace attr
    {
        enum AttributeType
        {
            mappedAddress = 0x01,
            username = 0x06,
            messageIntegrity = 0x08,
            errorCode = 0x09,
            unknownAttribute = 0x0a,
            realm = 0x14,
            nonce = 0x15,
            xorMappedAddress = 0x20,

            software = 0x8022,
            alternateServer = 0x8023,
            fingerprint = 0x8028,

            userDefine = 0x9000 , //!<we need this to identify our private extended stun attr
            unknown = 0xFFFF
        };

        class Attribute
        {
        public:
            virtual ~Attribute() {}
            virtual int type() const = 0;
        };

        /*!
            \note All values are decoded
        */
        class XorMappedAddress
        :
            public Attribute
        {
        public:
            static const AttributeType s_type = AttributeType::xorMappedAddress;
            virtual int type() const override { return s_type; }

            enum {
                IPV4 = 1,
                IPV6
            };
            int family;
            int port;

            union Ipv6
            {
                struct {
                    uint64_t hi;
                    uint64_t lo;
                } numeric;
                uint16_t array[8];
            };
            union
            {
                uint32_t ipv4;
                Ipv6 ipv6;
            } address;  //!< address in host byte order

            XorMappedAddress();
            XorMappedAddress( int port_, uint32_t ipv4_ );
            XorMappedAddress( int port_, Ipv6 ipv6_ );
        };

        //!ERROR-CODE attribute [rfc5389, 15.6]
        class ErrorDescription
        :
            public Attribute
        {
        public:
            ErrorDescription( int code_, std::string phrase = std::string() );
            static const AttributeType s_type = AttributeType::errorCode;
            virtual int type() const override { return s_type; }

            //!This value is full error code
            int code;
            //!utf8 string, limited to 127 characters
            std::string reasonPhrase;

            inline int getClass() const { return code / 100; }
            inline int getNumber() const { return code % 100; }
        };

        class FingerPrint
        :
            public Attribute
        {
        public:
            FingerPrint( uint32_t crc32_ );
            static const AttributeType s_type = AttributeType::fingerprint;
            virtual int type() const override { return s_type; }

            uint32_t crc32;
        };


        class MessageIntegrity
        :
            public Attribute
        {
        public:
            static const AttributeType s_type = AttributeType::messageIntegrity;
            virtual int type() const override { return s_type; }

            static const int SHA1_HASH_SIZE = 20;
            // for me to avoid raw loop when doing copy.
            std::array<uint8_t,SHA1_HASH_SIZE> hmac;
        };

        class UnknownAttribute
        :
            public Attribute
        {
        public:
            UnknownAttribute( int userType_, nx::Buffer value_ );
            static const AttributeType s_type = AttributeType::unknownAttribute;
            virtual int type() const override { return userType; }

            int userType;
            nx::Buffer value;
        };

        class UnknownAttributes
        {
        public:
            std::vector<int> attributeTypes;
        };


        typedef std::string StringAttributeType;
        bool parse( const UnknownAttribute& unknownAttr, StringAttributeType* val );
        bool serialize( UnknownAttribute* unknownAttr, const StringAttributeType& val , int userType );
    }

    // A specialized hash class for C++11 since it doesn't comes with built-in enum hash
    // function. Not a specialized template for hash class in namespace std .
    template< typename T > struct StunHash {};
    template<> struct StunHash<attr::AttributeType> {
        std::size_t operator () ( const attr::AttributeType& a ) const noexcept {
            return static_cast<std::size_t>(a);
        }
    };

    class Message
    {
    public:
        Header header;
        typedef std::unordered_multimap<int, std::unique_ptr<attr::Attribute> > AttributesMap;
        AttributesMap attributes;

        explicit Message( Header header = Header() );

        Message( Message&& message );
        Message& operator = ( Message&& message );

        void clear();

        // TODO: Uncomment when variadic templates are supported
        //
        // template< typename T, typename ... Args >
        // void addAttribute(Args ... args)
        // { addAttribute( std::make_unique< T >( args ) ); }

        void addAttribute( std::unique_ptr<attr::Attribute>&& attribute );

        template< typename T >
        const T* getAttribute( int aType = T::s_type, size_t index = 0 )
        {
            auto it = attributes.find( aType );
            if( it == attributes.end() )
                return nullptr;

            while( index )
            {
                ++it;
                --index;
                if( it == attributes.end() || it->first != aType )
                    return nullptr;
            }

            return static_cast<T*>( it->second.get() );
        }

    private:
        Message( const Message& );
        Message& operator=( const Message& );
    };

}

#endif  //STUN_MESSAGE_H
