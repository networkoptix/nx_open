/**********************************************************
* 21 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef HTTPTYPES_H
#define HTTPTYPES_H

#include <cstring>
#include <functional>
#include <map>

#include <QtCore/QByteArray>
#include <QtCore/QMap>
#include <QtCore/QUrl>

#include "utils/network/buffer.h"

#include "qnbytearrayref.h"


/*!
    Holds http-implementation suitable for async/sync i/o.
    Common structures, parsers/serializers, http client.

    All classes use QByteArray for representing http headers and their values for following reasons:\n
        - HTTP headers are coded using 8-bit ASCII and since QString stores data as 16-bit chars we would need additional data 
            conversion/copying to serialize header data to buffer, ready to be passed to socket
        - QByteArray is compatible with QString
*/
namespace nx_http
{
    const int DEFAULT_HTTP_PORT = 80;

    /*!
        TODO consider using another container.
        Need some buffer with:\n
            - implicit sharing
            - ability to hold reference to part of the source string (to perform substr with no allocation)
            - concatenate without relocation (hold list of buffers inside). This is suitable since socket write operation can accept array of buffers
            - pop_front operation with no relocation (just be moving internal offset)

        Using QByteArray or any STL container results in many memory copy/relocation operations which make implementation not effecient
    */
    typedef nx::Buffer BufferType;
    typedef QnByteArrayConstRef ConstBufferRefType;
    //TODO #ak not sure, if nx::Buffer is suitable for string type
    typedef nx::Buffer StringType;

    /*!
        \return < 0, if \a one < \a two. 0 if \a one == \a two. > 0 if \a one > \a two
    */
    int strcasecmp( const StringType& one, const StringType& two );

    /************************************************************************/
    /* Comparator for case-insensitive comparison in STL assos. containers  */
    /************************************************************************/
    struct ci_less : std::binary_function<QByteArray, QByteArray, bool>
    {
        // case-independent (ci) compare_less binary function
        bool operator() (const QByteArray& c1, const QByteArray& c2) const {
            return strcasecmp( c1, c2 ) < 0;
        }
    };

    //!Http header container
    /*!
        \warning This is multimap(!) to allow same header be present multiple times in single http message. To insert or replace use \a nx_http::insertOrReplace
    */
    typedef std::multimap<StringType, StringType, ci_less> HttpHeaders;
    typedef HttpHeaders::value_type HttpHeader;

    /*!
        This is convinient method for simplify transition from QHttp
        \return Value of header \a headerName (if found), empty string otherwise
    */
    StringType getHeaderValue( const HttpHeaders& headers, const StringType& headerName );

    //!convenient function for inserting or replacing header
    /*!
        \return iterator of added element
    */
    HttpHeaders::iterator insertOrReplaceHeader( HttpHeaders* const headers, const HttpHeader& newHeader );

    void removeHeader( HttpHeaders* const headers, const StringType& headerName );



    static const size_t BufferNpos = size_t(-1);


    /*!
        Searches first occurence of any element of \0-terminated string \a toSearch in \a count elements of \a str, starting with element \a offset
        \param toSearch \0-terminated string
        \param count number of characters of \a str to check, NOT a length of searchable characters string (\a toSearch)
        \note following algorithms differ from stl analogue in following: they limit number of characters checked during search 
    */
    template<class Str>
        size_t find_first_of(
            const Str& str,
            const typename Str::value_type* toSearch,
            size_t offset = 0,
            size_t count = BufferNpos )
    {
        const size_t toSearchDataSize = strlen( toSearch );
        const typename Str::value_type* strEnd = str.data() + (count == BufferNpos ? str.size() : (offset + count));
        for( const typename Str::value_type*
            curPos = str.data() + offset;
            curPos < strEnd;
            ++curPos )
        {
            if( memchr( toSearch, *curPos, toSearchDataSize ) )
                return curPos-str.data();
        }

        return BufferNpos;
    }

    template<class Str>
        size_t find_first_not_of(
            const Str& str,
            const typename Str::value_type* toSearch,
            size_t offset = 0,
            size_t count = BufferNpos )
    {
        const size_t toSearchDataSize = strlen( toSearch );
        const typename Str::value_type* strEnd = str.data() + (count == BufferNpos ? str.size() : (offset + count));
        for( const typename Str::value_type*
            curPos = str.data() + offset;
            curPos < strEnd;
            ++curPos )
        {
            if( memchr( toSearch, *curPos, toSearchDataSize ) == NULL )
                return curPos-str.data();
        }

        return BufferNpos;
    }

    template<class Str>
        size_t find_last_not_of(
            const Str& str,
            const typename Str::value_type* toSearch,
            size_t offset = 0,
            size_t count = BufferNpos )
    {
        const size_t toSearchDataSize = strlen( toSearch );
        const typename Str::value_type* strEnd = str.data() + (count == BufferNpos ? str.size() : (offset + count));
        for( const typename Str::value_type*
            curPos = strEnd-1;
            curPos >= str.data();
            --curPos )
        {
            if( memchr( toSearch, *curPos, toSearchDataSize ) == NULL )
                return curPos-str.data();
        }

        return BufferNpos;
    }

    template<class Str, class StrValueType>
        size_t find_last_of(
            const Str& str,
            const StrValueType toSearch,
            size_t offset = 0,
            size_t count = BufferNpos )
    {
        const StrValueType* strEnd = str.data() + (count == BufferNpos ? str.size() : (offset + count));
        for( const StrValueType*
            curPos = strEnd-1;
            curPos >= str.data();
            --curPos )
        {
            if( toSearch == *curPos )
                return curPos-str.data();
        }

        return BufferNpos;
    }


    //!Parses \a data and saves header name and data to \a *headerName and \a *headerValue
    bool parseHeader(
        StringType* const headerName,
        StringType* const headerValue,
        const ConstBufferRefType& data );
    bool parseHeader(
        ConstBufferRefType* const headerName,
        ConstBufferRefType* const headerValue,
        const ConstBufferRefType& data );
    HttpHeader parseHeader( const ConstBufferRefType& data );

    namespace StatusCode
    {
        /*!
            enum has name "Value" for unification purpose only. Real enumeration name is StatusCode (name of namespace). 
            Enum is referred to as StatusCode::Value, if assign real name to enum it will be StatusCode::StatusCode.
            Namespace is used to associate enum with corresponding functions like toString() and fromString().
            Namespace is required, since not all such functions can be put to global namespace. 
            E.g. this namespace has convenience function toString( int httpStatusCode )
        */
        enum Value
        {
            undefined = 0,
            _continue = 100,
            ok = 200,
            noContent = 204,
            partialContent = 206,
            multipleChoices = 300,
            moved_permanently = 301,
            moved = 302,
            badRequest = 400,
            unauthorized = 401,
            forbidden = 403,
            notFound = 404,
            notAllowed = 405,
            proxyAuthenticationRequired = 407,
            internalServerError = 500,
            notImplemented = 501,
            serviceUnavailable = 503
        };

        StringType toString( Value );
        StringType toString( int );
    };

    namespace Method
    {
        extern const StringType GET;
        extern const StringType POST;
        extern const StringType PUT;
    }

    //!Represents string like HTTP/1.1, RTSP/1.0
    class MimeProtoVersion
    {
    public:
        StringType protocol;
        StringType version;

        bool parse( const ConstBufferRefType& data );
        //!Appends serialized data to \a dstBuffer
        void serialize( BufferType* const dstBuffer ) const;

        bool operator==( const MimeProtoVersion& right ) const
        {
            return protocol == right.protocol && version == right.version;
        }
    };

    static const MimeProtoVersion http_1_0 = { "HTTP", "1.0" };
    static const MimeProtoVersion http_1_1 = { "HTTP", "1.1" };
    static const MimeProtoVersion rtsp_1_0 = { "RTSP", "1.0" };

    class RequestLine
    {
    public:
        StringType method;
        QUrl url;
        MimeProtoVersion version;

        bool parse( const ConstBufferRefType& data );
        //!Appends serialized data to \a dstBuffer
        void serialize( BufferType* const dstBuffer ) const;
    };

    class StatusLine
    {
    public:
        MimeProtoVersion version;
        int statusCode;
        StringType reasonPhrase;

        StatusLine();
        bool parse( const ConstBufferRefType& data );
        //!Appends serialized data to \a dstBuffer
        void serialize( BufferType* const dstBuffer ) const;
    };

    class Request
    {
    public:
        RequestLine requestLine;
        HttpHeaders headers;
        BufferType messageBody;

        bool parse( const ConstBufferRefType& data );
        //!Appends serialized data to \a dstBuffer
        void serialize( BufferType* const dstBuffer ) const;
    };

    class Response
    {
    public:
        StatusLine statusLine;
        HttpHeaders headers;
        BufferType messageBody;

        bool parse( const ConstBufferRefType& data );
        //!Appends serialized data to \a dstBuffer
        void serialize( BufferType* const dstBuffer ) const;
        BufferType toString() const;
    };

    namespace MessageType
    {
        // TODO: #Elric #enum
        enum Value
        {
            none,
            request,
            response
        };

        QLatin1String toString( Value val );
    }

    class Message
    {
    public:
        MessageType::Value type;
        union
        {
            Request* request;
            Response* response;
        };

        Message( MessageType::Value _type = MessageType::none );
        Message( const Message& right );
        Message( Message&& right );
        ~Message();

        Message& operator=(const Message& right);
        Message& operator=(Message&& right);

        void serialize( BufferType* const dstBuffer ) const;

        void clear();
        HttpHeaders& headers() { return type == MessageType::request ? request->headers : response->headers; };
        const HttpHeaders& headers() const { return type == MessageType::request ? request->headers : response->headers; };
        BufferType toString() const;
    };

    //!Contains http header structures
    namespace Header
    {
        //!Http authentication scheme enumeration
        namespace AuthScheme
        {
            // TODO: #Elric #enum
            enum Value
            {
                none,
                basic,
                digest
            };

            const char* toString( Value val );
            Value fromString( const char* str );
            Value fromString( const ConstBufferRefType& str );
        }

        //!Login/password to use in http authorization
        class UserCredentials
        {
        public:
            StringType userid;
            StringType password;
        };

        //!rfc2617, section 2
        class BasicCredentials
        :
            public UserCredentials
        {
        public:
            bool parse( const BufferType& str );
            void serialize( BufferType* const dstBuffer ) const;
        };

        //!rfc2617, section 3.2.2
        class DigestCredentials
        :
            public UserCredentials
        {
        public:
            QMap<BufferType, BufferType> params;

            bool parse( const BufferType& str );
            void serialize( BufferType* const dstBuffer ) const;
        };

        //!Authorization header (rfc2616, rfc2617)
        class Authorization
        {
        public:
            static const StringType NAME;

            AuthScheme::Value authScheme;
            union
            {
                BasicCredentials* basic;
                DigestCredentials* digest;
            };

            Authorization();
            Authorization( const AuthScheme::Value& authSchemeVal );
            ~Authorization();

            bool parse( const BufferType& str );
            void serialize( BufferType* const dstBuffer ) const;
            StringType toString() const;

        private:
            Authorization( const Authorization& );
            const Authorization& operator=( const Authorization& );
        };

        //!Convient class for generating Authorization header with Basic authentication method
        class BasicAuthorization
        :
            public Authorization
        {
        public:
            BasicAuthorization( const StringType& userName, const StringType& userPassword );
        };

        class DigestAuthorization
        :
            public Authorization
        {
        public:
            DigestAuthorization();

            void addParam( const BufferType& name, const BufferType& value );
        };

        class WWWAuthenticate
        {
        public:
            AuthScheme::Value authScheme;
            QMap<BufferType, BufferType> params;

            WWWAuthenticate();

            bool parse( const BufferType& str );
        };
    }

    typedef std::pair<StringType, StringType> ChunkExtension;

    //! chunk-size [ chunk-extension ] CRLF
    /*!
        chunk-extension= *( ";" chunk-ext-name [ "=" chunk-ext-val ] )
        chunk-ext-name = token
        chunk-ext-val  = token | quoted-string
    */
    class ChunkHeader
    {
    public:
        size_t chunkSize;
        std::vector<ChunkExtension> extensions;

        ChunkHeader();

        void clear();

        /*!
            \return bytes read from \a buf. -1 in case of parse error
            \note In case of parse error object state is indefined
        */
        int parse( const ConstBufferRefType& buf );
        /*!
            \return bytes written to \a dstBuffer. -1 in case of serialize error. In this case contents of \a dstBuffer are undefined
        */
        int serialize( BufferType* const dstBuffer ) const;
    };
}

#endif  //HTTPTYPES_H
